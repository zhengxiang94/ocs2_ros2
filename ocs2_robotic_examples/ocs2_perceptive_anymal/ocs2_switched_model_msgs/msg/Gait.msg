# Gait message

float32[] event_phases       # event phases: length equal to mode_sequence's - 1. Values within [0,1]
int8[]    mode_sequence      # mode sequence: e.g., for a quadrupedal robot, it is in the set {0, 1,..., 15}
float32 duration
