#-------------------------------------
# tests of the michi fix_atari routine
#-------------------------------------

# escape
# ------
debug setpos C8 C9 E9 B8 F9 D8
10 debug fix_atari C8
#? [1 C7]

debug setpos C1 G7 B2 B1
20 debug fix_atari B1
#? [1 A1]

play b e5
30 debug fix_atari B1
#? [1]

# counter capture
# ---------------
clear_board
debug setpos A1 E5 B2 A2
110 debug fix_atari A1
#? [1 A3 B1]

# shicho (ladder)
# ---------------
clear_board
debug setpos A1 A2
210 debug fix_atari A1
#? [1]

debug setpos G1
220 debug fix_atari A1
#? [1 B1]

debug setpos D2
230 debug fix_atari A1
#? [1]

clear_board
debug setpos G5 F5 A1 G4 A2 H4 A3 G6 H5
240 debug fix_atari G5
#? [0 H6|0 J5]

clear_board
debug setpos E5 D5 A1 E4 A2 F4 A3 E6 F5
250 debug fix_atari E5
#? [0 G5]

clear_board
debug setpos D3 F3 E3 G3 F2 E2 G2 H2 D2
260 debug fix_atari E2
#? [1]

