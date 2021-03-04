//
// Created by rgrandia on 09.11.20.
//

#include "ocs2_sqp/MultipleShootingSolver.h"
#include <ocs2_core/control/FeedforwardController.h>
#include <ocs2_core/misc/LinearInterpolation.h>
#include <ocs2_oc/approximate_model/ChangeOfInputVariables.h>
#include <ocs2_sqp/ConstraintProjection.h>
#include <ocs2_sqp/DynamicsDiscretization.h>
#include <Eigen/QR>
#include <chrono>
#include <iostream>

// scalar_t is double
// matrix_t is Eigen::MatrixXd
// matrix_array_t is std::vector<Eigen::MatrixXd>

namespace ocs2 {

MultipleShootingSolver::MultipleShootingSolver(MultipleShootingSolverSettings settings, const SystemDynamicsBase* systemDynamicsPtr,
                                               const CostFunctionBase* costFunctionPtr, const ConstraintBase* constraintPtr,
                                               const CostFunctionBase* terminalCostFunctionPtr)
    : SolverBase(),
      systemDynamicsPtr_(systemDynamicsPtr->clone()),
      costFunctionPtr_(costFunctionPtr->clone()),
      constraintPtr_(nullptr),
      terminalCostFunctionPtr_(nullptr),
      settings_(std::move(settings)),
      totalNumIterations_(0) {
  if (constraintPtr != nullptr) {
    constraintPtr_.reset(constraintPtr->clone());
  }

  if (terminalCostFunctionPtr != nullptr) {
    terminalCostFunctionPtr_.reset(terminalCostFunctionPtr->clone());
  }
}

MultipleShootingSolver::~MultipleShootingSolver() {
  if (settings_.printSolverStatistics) {
    std::cerr << getBenchmarkingInformation() << std::endl;
  }
}

void MultipleShootingSolver::reset() {
  // reset timers
  totalNumIterations_ = 0;
  linearQuadraticApproximationTimer_.reset();
  solveQpTimer_.reset();
  computeControllerTimer_.reset();
}

std::string MultipleShootingSolver::getBenchmarkingInformation() const {
  const auto linearQuadraticApproximationTotal = linearQuadraticApproximationTimer_.getTotalInMilliseconds();
  const auto solveQpTotal = solveQpTimer_.getTotalInMilliseconds();
  const auto computeControllerTotal = computeControllerTimer_.getTotalInMilliseconds();

  const auto benchmarkTotal = linearQuadraticApproximationTotal + solveQpTotal + computeControllerTotal;

  std::stringstream infoStream;
  if (benchmarkTotal > 0.0) {
    const scalar_t inPercent = 100.0;
    infoStream << "\n########################################################################\n";
    infoStream << "The benchmarking is computed over " << totalNumIterations_ << " iterations. \n";
    infoStream << "SQP Benchmarking\t   :\tAverage time [ms]   (% of total runtime)\n";
    infoStream << "\tLQ Approximation   :\t" << linearQuadraticApproximationTimer_.getAverageInMilliseconds() << " [ms] \t\t("
               << linearQuadraticApproximationTotal / benchmarkTotal * inPercent << "%)\n";
    infoStream << "\tSolve QP           :\t" << solveQpTimer_.getAverageInMilliseconds() << " [ms] \t\t("
               << solveQpTotal / benchmarkTotal * inPercent << "%)\n";
    infoStream << "\tCompute Controller :\t" << computeControllerTimer_.getAverageInMilliseconds() << " [ms] \t\t("
               << computeControllerTotal / benchmarkTotal * inPercent << "%)\n";
  }
  return infoStream.str();
}

void MultipleShootingSolver::runImpl(scalar_t initTime, const vector_t& initState, scalar_t finalTime,
                                     const scalar_array_t& partitioningTimes) {
  if (settings_.printSolverStatus) {
    std::cerr << "\n++++++++++++++++++++++++++++++++++++++++++++++++++++++";
    std::cerr << "\n+++++++++++++ SQP solver is initialized ++++++++++++++";
    std::cerr << "\n++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";
  }

  // STATE_DIM & INPUT_DIM are fixed, can be retrieved from the Q, R matrices from task.info

  // ignore partitioningTimes

  // Initialize cost
  costFunctionPtr_->setCostDesiredTrajectoriesPtr(&this->getCostDesiredTrajectories());
  if (terminalCostFunctionPtr_) {
    terminalCostFunctionPtr_->setCostDesiredTrajectoriesPtr(&this->getCostDesiredTrajectories());
  }

  // EQ_CONSTRAINT_DIM is not fixed in different modes, get them
  auto timeDiscretization = getInfoFromModeSchedule(initTime, finalTime);
  const int N = static_cast<int>(timeDiscretization.size()) - 1;

  // Initialize the state and input containers
  std::vector<ocs2::vector_t> x(N + 1, ocs2::vector_t(settings_.n_state));
  std::vector<ocs2::vector_t> u(N, ocs2::vector_t(settings_.n_input));
  if (totalNumIterations_ == 0) {  // first iteration
    for (int i = 0; i < N; i++) {
      x[i] = initState;
      u[i] = vector_t::Zero(settings_.n_input);
    }
    x[N] = initState;
  } else {
    // previous N_real may not be the same as the current one, so size mismatch might happen.
    // do linear interpolation here
    x[0] = initState;  // Force linearization of the first node around the current state
    for (int i = 1; i < (N + 1); i++) {
      x[i] = LinearInterpolation::interpolate(timeDiscretization[i], primalSolution_.timeTrajectory_, primalSolution_.stateTrajectory_);
    }
    for (int i = 0; i < N; i++) {
      u[i] = LinearInterpolation::interpolate(timeDiscretization[i], primalSolution_.timeTrajectory_, primalSolution_.inputTrajectory_);
    }
  }

  for (int iter = 0; iter < settings_.sqpIteration; iter++) {
    if (settings_.printSolverStatus) {
      std::cerr << "SQP iteration: " << iter << "\n";
    }
    // Make QP approximation
    linearQuadraticApproximationTimer_.startTimer();
    setupCostDynamicsEqualityConstraint(*systemDynamicsPtr_, *costFunctionPtr_, constraintPtr_.get(), terminalCostFunctionPtr_.get(),
                                        timeDiscretization, x, u);
    linearQuadraticApproximationTimer_.endTimer();

    // Solve QP
    solveQpTimer_.startTimer();
    vector_t delta_x0 = initState - x[0];
    std::vector<ocs2::vector_t> delta_x;
    std::vector<ocs2::vector_t> delta_u;
    std::tie(delta_x, delta_u) = getOCPSolution(delta_x0);
    solveQpTimer_.endTimer();

    // Apply step
    // TODO implement line search
    computeControllerTimer_.startTimer();
    scalar_t deltaUnorm = 0.0;
    scalar_t deltaXnorm = 0.0;
    for (int i = 0; i < N; i++) {
      deltaXnorm += delta_x[i].norm();
      x[i] += delta_x[i];
      deltaUnorm += delta_u[i].norm();
      u[i] += delta_u[i];
    }
    x[N] += delta_x[N];

    totalNumIterations_++;
    if (deltaUnorm < settings_.deltaTol && deltaXnorm < settings_.deltaTol) {
      break;
    }
  }

  // Store result in PrimalSolution. time, state , input
  primalSolution_.timeTrajectory_ = std::move(timeDiscretization);
  primalSolution_.stateTrajectory_ = std::move(x);
  primalSolution_.inputTrajectory_ = std::move(u);
  primalSolution_.inputTrajectory_.push_back(primalSolution_.inputTrajectory_.back());  // repeat last input to make equal length vectors
  primalSolution_.modeSchedule_ = this->getModeSchedule();
  primalSolution_.controllerPtr_.reset(new FeedforwardController(primalSolution_.timeTrajectory_, primalSolution_.inputTrajectory_));

  computeControllerTimer_.endTimer();

  if (settings_.printSolverStatus) {
    std::cerr << "\n++++++++++++++++++++++++++++++++++++++++++++++++++++++";
    std::cerr << "\n+++++++++++++ SQP solver has terminated ++++++++++++++";
    std::cerr << "\n++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";
  }
}

std::pair<std::vector<ocs2::vector_t>, std::vector<ocs2::vector_t>> MultipleShootingSolver::getOCPSolution(const vector_t& delta_x0) {
  // Prepare solver size
  hpipmInterface_.resize(ocpSize_);

  // Solve the QP
  std::vector<ocs2::vector_t> deltaXSol;
  std::vector<ocs2::vector_t> deltaUSol;
  if (constraintPtr_ && !settings_.qr_decomp) {
    hpipmInterface_.solve(delta_x0, dynamics_, cost_, &constraints_, deltaXSol, deltaUSol, settings_.printSolverStatus);
  } else {  // without constraints, or when using QR decomposition, we have an unconstrained QP.
    hpipmInterface_.solve(delta_x0, dynamics_, cost_, nullptr, deltaXSol, deltaUSol, settings_.printSolverStatus);
  }

  // remap the tilde delta u to real delta u
  if (constraintPtr_ && settings_.qr_decomp) {
    for (int i = 0; i < deltaUSol.size(); i++) {
      deltaUSol[i] = constraints_[i].dfdu * deltaUSol[i];  // creates a temporary because of alias
      deltaUSol[i].noalias() += constraints_[i].dfdx * deltaXSol[i];
      deltaUSol[i] += constraints_[i].f;
    }
  }

  return {deltaXSol, deltaUSol};
}

void MultipleShootingSolver::setupCostDynamicsEqualityConstraint(SystemDynamicsBase& systemDynamics, CostFunctionBase& costFunction,
                                                                 ConstraintBase* constraintPtr, CostFunctionBase* terminalCostFunctionPtr,
                                                                 const scalar_array_t& time, const std::vector<ocs2::vector_t>& x,
                                                                 const std::vector<ocs2::vector_t>& u) {
  // x of shape (n_state, N_real + 1), n = n_state;
  // u of shape (n_input, N_real), m = n_input;
  // Matrix pi of shape (n_state, N), this is not used temporarily
  // N_real is the horizon length
  const int N = static_cast<int>(time.size()) - 1;

  // Set up for constant state input size. Will be adapted based on constraint handling.
  ocpSize_ = HpipmInterface::OcpSize(N, settings_.n_state, settings_.n_input);

  dynamics_.resize(N);
  cost_.resize(N + 1);
  constraints_.resize(N + 1);
  for (int i = 0; i < N; i++) {
    const scalar_t ti = time[i];
    const scalar_t dt = time[i + 1] - time[i];

    // Dynamics
    // Discretization returns // x_{k+1} = A_{k} * dx_{k} + B_{k} * du_{k} + b_{k}
    dynamics_[i] = rk4Discretization(systemDynamics, ti, x[i], u[i], dt);
    dynamics_[i].f -= x[i + 1];  // make it dx_{k+1} = ...

    // Costs: Approximate the integral with forward euler
    cost_[i] = costFunction.costQuadraticApproximation(ti, x[i], u[i]);
    cost_[i].dfdxx *= dt;
    cost_[i].dfdux *= dt;
    cost_[i].dfduu *= dt;
    cost_[i].dfdx *= dt;
    cost_[i].dfdu *= dt;
    cost_[i].f *= dt;

    if (constraintPtr != nullptr) {
      // C_{k} * dx_{k} + D_{k} * du_{k} + e_{k} = 0
      // C_{k} = constraintApprox.dfdx
      // D_{k} = constraintApprox.dfdu
      // e_{k} = constraintApprox.f
      constraints_[i] = constraintPtr->stateInputEqualityConstraintLinearApproximation(ti, x[i], u[i]);
      if (settings_.qr_decomp) {
        // Handle equality constraints using QR decomposition.

        // reduces number of inputs
        ocpSize_.nu[i] = settings_.n_input - constraints_[i].f.rows();
        // Projection stored instead of constraint, // TODO: benchmark between lu and qr method. LU seems slightly faster.
        constraints_[i] = luConstraintProjection(constraints_[i]);

        // Adapt dynamics and cost
        changeOfInputVariables(dynamics_[i], constraints_[i].dfdu, constraints_[i].dfdx, constraints_[i].f);
        changeOfInputVariables(cost_[i], constraints_[i].dfdu, constraints_[i].dfdx, constraints_[i].f);
      } else {
        // Declare as general inequalities
        ocpSize_.ng[i] = constraints_[i].f.rows();
      }
    }
  }

  if (terminalCostFunctionPtr != nullptr) {
    cost_[N] = terminalCostFunctionPtr->finalCostQuadraticApproximation(time[N], x[N]);
  } else {
    cost_[N] = ScalarFunctionQuadraticApproximation::Zero(settings_.n_state, 0);
  }
}

scalar_array_t MultipleShootingSolver::getInfoFromModeSchedule(scalar_t initTime, scalar_t finalTime) {
  /*
  A simple example here illustrates the mission of this function

  Assume:
    this->getModeSchedule().eventTimes = {3.25, 3.4, 3.88, 4.02, 4.5}
    initTime = 3.0
    finalTime = 4.0
    user_defined delta_t = 0.1

  Then the following variables will be:
    trueEventTimes = {3.0, 3.1, 3.2, 3.25, 3.35, 3.4, 3.5, 3.6, 3.7, 3.8, 3.88, 3.98, 4.0}
  */
  scalar_array_t tempEventTimes = this->getModeSchedule().eventTimes;
  scalar_t delta_t = (finalTime - initTime) / settings_.N;

  if (settings_.printModeScheduleDebug) {
    std::cout << "event times original \n";
    for (int i = 0; i < tempEventTimes.size(); i++) {
      std::cout << "event time " << i << " is " << tempEventTimes[i] << std::endl;
    }
  }

  tempEventTimes.insert(tempEventTimes.begin(), initTime);
  for (int i = 0; i < tempEventTimes.size(); i++) {
    if (std::abs(tempEventTimes[i] - finalTime) < std::numeric_limits<double>::epsilon() || tempEventTimes[i] > finalTime) {
      tempEventTimes.erase(tempEventTimes.begin() + i, tempEventTimes.end());
      break;
    }
  }
  tempEventTimes.push_back(finalTime);

  if (settings_.printModeScheduleDebug) {
    std::cout << "event times after \n";
    for (int i = 0; i < tempEventTimes.size(); i++) {
      std::cout << "event time " << i << " is " << tempEventTimes[i] << std::endl;
    }
  }

  size_t n_mode = tempEventTimes.size() - 1;
  size_array_t N_distribution;
  N_distribution.resize(n_mode);

  size_t N_distribution_sum = 0;
  for (int i = 0; i < n_mode; i++) {
    scalar_t division_result = (tempEventTimes[i + 1] - tempEventTimes[i]) / delta_t;
    scalar_t intpart;
    scalar_t fractpart = std::modf(division_result, &intpart);
    auto N_distribution_i = static_cast<size_t>(intpart);
    if (std::abs(fractpart) < 1e-4) {
      N_distribution_i -= 1;
    }
    N_distribution[i] = N_distribution_i;
    if (settings_.printModeScheduleDebug) {
      std::cout << "mode " << i << " has distribution " << N_distribution_i << std::endl;
      std::cout << "division result: " << division_result << " intpart: " << intpart << " fracpart:" << fractpart << std::endl;
    }
    N_distribution_sum += N_distribution_i;
  }

  scalar_array_t trueEventTimes;
  for (int i = 0; i < n_mode; i++) {
    for (int j = 0; j < N_distribution[i] + 1; j++) {
      trueEventTimes.push_back(tempEventTimes[i] + delta_t * j);
    }
  }
  trueEventTimes.push_back(finalTime);

  if (settings_.printModeScheduleDebug) {
    std::cout << "true event times: {";
    for (const auto t : trueEventTimes) {
      std::cout << t << ", ";
    }
    std::cout << "}\n";
  }
  return trueEventTimes;
}

}  // namespace ocs2