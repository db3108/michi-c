#-------------------------------------------
# tests of the michi large patterns routines
#-------------------------------------------

# size 5 center pattern
# ---------------------
debug setpos D6 E6 D5 E5 D4 E3 F6 A1 F5 A2 F4 A3
10 debug match_pat E4
#? [410926]

# size 4 side pattern
# -------------------
clear_board
debug setpos D1 D2 D3 C2 E3 F3 E4 F1
20 debug match_pat E2
#? [923280]

# Idem with 90 deg rotation
# -------------------------
clear_board
debug setpos A5 B5 C5 B6 C4 C3 D4 A3
30 debug match_pat B4
#? [923280]

# Idem with 180 deg rotation
# --------------------------
clear_board
debug setpos F13 F12 F11 G12 E11 D11 E10 D13
40 debug match_pat E12
#? [923280]

# Idem with 270 deg rotation
# --------------------------
clear_board
debug setpos N8  M8  L8  M7  L9  L10 K9  N10
50 debug match_pat M9 
#? [923280]

# Idem with vertical flip
# -----------------------
clear_board
debug setpos J1 J2 J3 K2 H3 G3 H4 G1
60 debug match_pat H2
#? [923280]

# Large pattern in the corner
# ---------------------------
clear_board
debug setpos B2 A2 C3 B3 D3 C2 D2 C4 E2 D4 F2 E4 F3 F4 F1 E3 G2 G3
70 debug match_pat B1 
#? [125951] 
