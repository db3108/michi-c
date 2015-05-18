// patterns.c -- Routines for 3x3 patterns and large patterns for michi program
//
// (c) 2015 Denis Blumstein <db3108@free.fr> Petr Baudis <pasky@ucw.cz>
// MIT licence (i.e. almost public domain)
#include "michi.h"

// There are two types of patterns used in michi :
//
// - 3x3 patterns : given the color of the 8 neighbors of the central point 
//   (4 neighbors and 4 diagonal neighbors) the pat3_match() function returns 
//   the answer (yes/no) to the question : 
//   Is there a 3x3 pattern matching at this point ?
//   The recognized patterns are defined by the table pat3_src[] below
//
// - Large patterns : given the color of the points in a certain neighborhood 
//   of the central point, the large_pattern_probability() function returns the
//   probability of play at this position. 
//   This probability is used to bias the search in the MCTS tree. 
//
//   The point neighborhoods are defined in [4] (see references in michi.c). 
//   They are symetric under reflexions and rotations of the board. For each
//   point, 12 neighborhoods of increasing size are considered, each 
//   neighborhood includes all the neighborhoods of lesser size.
//   In the program, the neighborhoods are defined by the 2 tables :
//   pat_gridcular_seq and pat_gridcular_size. These tables are compiled into 
//   the pat_gridcular_seq1d array.

// See below for more details on the pattern code.
//
// ================================ 3x3 patterns ==============================
//
// 1 bit is sufficient to store the fact that a pattern matches for a given 
// configuration of the 8 neighbors color. This configuration can be encoded 
// as a 16 bits integer called env8 (as 2 bits are sufficient to encode one of
// the 4 possible colors of a point). So the set of 3x3 patterns that are 
// recognized is represented by an array of 65536 bits (or 8192 bytes).
//
// The used patterns are symetrical wrt the color so we only need a single array
// (pat3set). A bit is set in this array when at least one pattern matches.
//
// These patterns are used in the random playouts, so pattern matching must be
// very fast. To achieve best speed the 2 components of env8, env4 and env4d,
// are stored as arrays in the struct Position and are incrementally updated by
// the routines that modify the board put_stone() and remove_stone()
//
// Note: as the patterns are symetrical wrt to the color, we do not care to 
//       reverse the color in env4 and env4d after each move. The env4[] and 
//       env4d[] are defined in terms of BLACK and WHITE rather than 'X' or 'x' 
//
Byte bit[8]={1,2,4,8,16,32,64,128};
Byte pat3set[8192];     // Set of the pat3 patterns (defined in pat3src below)
int  npat3;             // Number of patterns in pat3src

// A set of 3x3 patterns is defined by an array of ASCII strings (of length 10) 
// with the following encoding
// char pat_src[][10]= {
//       "XOX"                   // X : one of BLACK or WHITE    
//       "..."                   // O : the other color 
//       "???",                  // . : EMPTY
//       "XO."                   // x : not X i.e O or EMPTY or OUT
//       ".X."                   // o : not O i.e X or EMPTY or OUT
//       "?.?",                  // ? : BLACK or WHITE or EMPTY or OUT
//       "###"                   // # : edge of the goban (out of board)
//       "###"
//       "###"                   // string that mark the end of input
// }

char pat3src[][10] = { 
    "XOX"   // 1- hane pattern - enclosing hane
    "..."
    "???",
    "XO."   // 2- hane pattern - non-cutting hane
    "..." 
    "?.?",
    "XO?"   // 3- hane pattern - magari
    "X.." 
    "x.?",
    //"XOO",  // hane pattern - thin hane
    //"...",
    //"?.?", "X",  - only for the X player
    ".O."   // 4- generic pattern - katatsuke or diagonal attachment; 
            //similar to magari
    "X.." 
    "...",
    "XO?"   // 5- cut1 pattern (kiri] - unprotected cut
    "O.o" 
    "?o?",
    "XO?"   // 6- cut1 pattern (kiri] - peeped cut
    "O.X" 
    "???",
    "?X?"   // 7- cut2 pattern (de]
    "O.O" 
    "ooo",
    "OX?"   // 8- cut keima
    "o.O" 
    "???",
    "X.?"   // 9- side pattern - chase
    "O.?" 
    "##?",
    "OX?"   // 10- side pattern - block side cut
    "X.O" 
    "###",
    "?X?"   // 11- side pattern - block side connection
    "x.O" 
    "###",
    "?XO"   // 12- side pattern - sagari
    "x.x" 
    "###",
    "?OX"   // 13- side pattern - cut
    "X.O" 
    "###",
    "###" 
    "###" 
    "###"  // Mark the end of the pattern list
};

int nb=0;

int code(char color, int p)
{
    // Bits set for the 4 neighbours North(1), East(5), South(7), West(3)
    // or for the 4 diagonal neighbours NE(2), SE(8), SW(6), NE(0)
    int code_W[4] = {   0,    0,    0,    0};           // WHITE(O)
    int code_B[4] = {0x01, 0x02, 0x04, 0x08};           // BLACK(X)
    int code_E[4] = {0x10, 0x20, 0x40, 0x80};           // EMPTY(.)
    int code_O[4] = {0x11, 0x22, 0x44, 0x88};           // OUT  (#)
    switch(color) {
        case 'O': return code_W[p];
        case 'X': return code_B[p];
        case '.': return code_E[p];
        case '#': return code_O[p];
    }
    return 0;       // can't happen, but make compiler happy
}

int compute_code(char *src)
// Compute a 16 bits code that completely describes the 3x3 environnement of a
// given point.
// Note: the low 8 bits describe the state of the 4 neighbours, 
//       the high 8 bits describe the state of the 4 diagonal neighbors
{
                                        // src   0 1 2     bits in env8  7 0 4
    int env8=0;                         //       3 4 5     bit 0=LSB     3 . 1
                                        //       6 7 8                   6 2 5
    // Neighbours of the central point
    env8 |= code(src[1], 0);    // North value given by src[1] in position 0
    env8 |= code(src[5], 1);    // East  value given by src[5] in position 1
    env8 |= code(src[7], 2);    // South value given by src[7] in position 2
    env8 |= code(src[3], 3);    // West  value given by src[3] in position 3
    // Diagonal neighbours of the central point
    env8 |= code(src[2], 0)<<8;// North/East value given by src[2] in position 0
    env8 |= code(src[8], 1)<<8;// South/East value given by src[8] in position 1
    env8 |= code(src[6], 2)<<8;// South/West value given by src[6] in position 2
    env8 |= code(src[0], 3)<<8;// North/West value given by src[0] in position 3
    return env8;
}

int pat;
void pat_wildexp(char *src, int i)
// Expand wildchar in src[i]
{
    char src1[10];
    int env8;
    if ( i==9 ) { // all the positions in src are processed -- end of recursion
        env8 = compute_code(src);
        nb++;
        int q = env8 >> 3, r = env8 & 7;
        pat3set[q] |= bit[r];              // set the bit corresponding to env8
        return;
    }
    if (src[i] == '?') {
        strcpy(src1, src);
        src1[i] = 'X'; pat_wildexp(src1, i+1);
        src1[i] = 'O'; pat_wildexp(src1, i+1);
        src1[i] = '.'; pat_wildexp(src1, i+1);
        src1[i] = '#'; pat_wildexp(src1, i+1);
    }
    else if (src[i] == 'x') {
        strcpy(src1, src);
        src1[i]='O'; pat_wildexp(src1, i+1);
        src1[i]='.'; pat_wildexp(src1, i+1);
        src1[i]='#'; pat_wildexp(src1, i+1);
    }
    else if (src[i] == 'o') {
        strcpy(src1, src);
        src1[i]='X'; pat_wildexp(src1, i+1);
        src1[i]='.'; pat_wildexp(src1, i+1);
        src1[i]='#'; pat_wildexp(src1, i+1);
    }
    else 
        pat_wildexp(src, i+1);
}

char *swapcolor(char *src) 
{
    for (int i=0 ; i<9 ; i++) {
        switch (src[i]) {
            case 'X': src[i] = 'O'; break;
            case 'O': src[i] = 'X'; break;
            case 'x': src[i] = 'o'; break;
            case 'o': src[i] = 'x'; break;
        } 
    }
    return src;
}

char* horizflip(char *src)
{
    SWAP(char, src[0], src[6]);
    SWAP(char, src[1], src[7]);
    SWAP(char, src[2], src[8]);
    return src;
}

char* vertflip(char *src)
{
    SWAP(char, src[0], src[2]);
    SWAP(char, src[3], src[5]);
    SWAP(char, src[6], src[8]);
    return src;
}

char* rot90(char *src)
{
    char t=src[0]; src[0]=src[2]; src[2]=src[8]; src[8]=src[6]; src[6]=t;
    t=src[1]; src[1]=src[5]; src[5]=src[7]; src[7]=src[3]; src[3]=t; 
    return src;
}

void pat_enumerate3(char *src)
{
    char src1[10];
    pat_wildexp(src, 0);
    strcpy(src1,src);
    pat_wildexp(swapcolor(src1), 0);
}

void pat_enumerate2(char *src)
{
    char src1[10];
    pat_enumerate3(src);
    strcpy(src1, src);
    pat_enumerate3(horizflip(src1));
}

void pat_enumerate1(char *src)
{
    char src1[10];
    pat_enumerate2(src);
    strcpy(src1, src);
    pat_enumerate2(vertflip(src1));
}

void pat_enumerate(char *src)
{
    char src1[10];
    pat_enumerate1(src);
    strcpy(src1, src);
    pat_enumerate1(rot90(src1));
}

void make_pat3set(void)
{
    npat3 = sizeof(pat3src) / 10 - 1;
    if (npat3 > 13) {
        fprintf(stderr,"Error npat3 too big (%d)\n", npat3);
        exit(-1);
    }
    memset(pat3set,0,8192);
    for(int p=0 ; strcmp(pat3src[p], "#########") != 0 ; p++) {
        pat = p;
        pat_enumerate(pat3src[p]);
    }
}

// =============================== Large patterns =============================
//
// The sizes of the neighborhoods are large (up to 141 points). Therefore the
// exact configuration of the colors in the neighborhoods cannot be used for
// pattern matching. Instead, a Zobrist signature (see [4] et [7]) of 64 bits
// is computed from all points in the neighborhoods. Then this signature is 
// searched in a big hash table that contains the signatures of the patterns
// read in the file patterns.spat. If successful, the search returns the 
// probability of the patterns. 
//
// A large board with 7 layers of OUT of board points is used in order to avoid
// tests during the computation of the signature for a given point. This large
// board contains only the information on the color of points. It is build by
// copy_to_large_board() which is called only once in the routine expand() while
// pat_match() is called many times. 
// 
// With the large board it is an easy matter to compute the signature by 
// looping on all the points of the neighborhood thanks to the gridcular_seq1d
// array which stores the displacements with respect to the central point.
//
// The hash table "patterns" is computed by init_patterns(). 
// It uses internal chaining with double hashing [9].
// The performance of this hash table is reported in the log file michi.log 
// after the compilation of patterns.spat file and at the end of the execution.
//
// ------------------------ Data Structures -----------------------------------
// Large pattern entry in the hash table
typedef struct hash_t {
    unsigned long key;      // 64 bits Zobrist hash
    int           id;       // id of the pattern
    float         prob;     // probability of move triggered by the pattern
} LargePat;
#define KSIZE      25         // key size in bits
#define LENGTH     (1<<KSIZE) // Size of the hash table
#define KMASK      (LENGTH-1) // Mask to get the key from the hash signature
#define FOUND      -1

// Displacements with respect to the central point
typedef struct shift_t { int x, y; } Shift;

//---------------------------- Global Data ------------------------------------
Shift pat_gridcular_seq[] = {
    {0,0},      // d=1,2,3 is not considered separately                size 1
    {0,1}, {0,-1}, {1,0}, {-1,0}, {1,1}, {-1,1}, {1,-1}, {-1,-1},
    {0,2}, {0,-2}, {2,0}, {-2,0},                                   // size 2
    {1,2}, {-1,2}, {1,-2}, {-1,-2}, {2,1}, {-2,1}, {2,-1}, {-2,-1}, // size 3
    {0,3}, {0,-3}, {2,2}, {-2,2}, {2,-2}, {-2,-2}, {3,0}, {-3,0},   // size 4
    {1,3}, {-1,3}, {1,-3}, {-1,-3}, {3,1}, {-3,1}, {3,-1}, {-3,-1}, // size 5
    {0,4}, {0,-4}, {2,3}, {-2,3}, {2,-3}, {-2,-3}, {3,2}, {-3,2},   // size 6
    {3,-2}, {-3,-2}, {4,0}, {-4,0},
    {1,4}, {-1,4}, {1,-4}, {-1,-4}, {3,3}, {-3,3}, {3,-3}, {-3,-3}, // size 7
    {4,1}, {-4,1}, {4,-1}, {-4,-1},
    {0,5}, {0,-5}, {2,4}, {-2,4}, {2,-4}, {-2,-4}, {4,2}, {-4,2},   // size 8
    {4,-2}, {-4,-2}, {5,0}, {-5,0},
    {1,5}, {-1,5}, {1,-5}, {-1,-5}, {3,4}, {-3,4}, {3,-4}, {-3,-4}, // size 9
    {4,3}, {-4,3}, {4,-3}, {-4,-3}, {5,1}, {-5,1}, {5,-1}, {-5,-1},
    {0,6}, {0,-6}, {2,5}, {-2,5}, {2,-5}, {-2,-5}, {4,4}, {-4,4},   // size 10
    {4,-4}, {-4,-4}, {5,2}, {-5,2}, {5,-2}, {-5,-2}, {6,0}, {-6,0},
    {1,6}, {-1,6}, {1,-6}, {-1,-6}, {3,5}, {-3,5}, {3,-5}, {-3,-5}, // size 11
    {5,3}, {-5,3}, {5,-3}, {-5,-3}, {6,1}, {-6,1}, {6,-1}, {-6,-1},
    {0,7}, {0,-7}, {2,6}, {-2,6}, {2,-6}, {-2,-6}, {4,5}, {-4,5},   // size 12
    {4,-5}, {-4,-5}, {5,4}, {-5,4}, {5,-4}, {-5,-4}, {6,2}, {-6,2},
    {6,-2}, {-6,-2}, {7,0}, {-7,0}
};
int pat_gridcular_seq1d[141];
int pat_gridcular_size[13] = {0,9,13,21,29,37,49,61,73,89,105,121,141};
int large_patterns_loaded = 0;
// Primes used for double hashing in find_pat()
int primes[32]={5,      11,    37,   103,   293,   991, 2903,  9931,
                7,      19,    73, 10009, 11149, 12553, 6229, 10181,
                1013, 1583,  2503,  3491,  4637,  5501, 6571,  7459,
                8513, 9433, 10433, 11447, 11887, 12409, 2221,  4073};

static char buf[512];
int      color[256];
unsigned long zobrist_hashdata[141][4];
LargePat *patterns;
float    *probs;
long     nsearchs=0;
long     nsuccess=0;
double   sum_len_success=0;
double   sum_len_failure=0;

char large_board[LARGE_BOARDSIZE];
int  large_coord[BOARDSIZE]; // coord in the large board of any points of board

// Code: ------ Dictionnary of patterns (hastable with internal chaining) -----
void print_pattern(const char *msg, int i, LargePat p)
{
    sprintf(buf,"%s%-6d %16.16lx %6d %f", msg, i, p.key, p.id, p.prob);
}

void dump_patterns()
{
    printf("Large patterns hash table\n");
    for (int i=0 ; i<LENGTH ; i++) {
        print_pattern("", i, patterns[i]);
        printf("%s\n", buf);
    }
}

int find_pat(unsigned long key)
{
    assert(key!=0);

    int h = (key>>20) & KMASK, h2=primes[(key>>(20+KSIZE)) & 15], len=0;
    nsearchs++;
    while (patterns[h].key != key) {
        len++;
        if (patterns[h].key == 0) {
            sum_len_failure += len;
            return h;
        }
        h+=h2; if (h>LENGTH) h -= LENGTH;
    }
    nsuccess++;
    sum_len_success += len;
    return h;
}

int insert_pat(LargePat p)
{
    int i = find_pat(p.key);
    if (patterns[i].key==0) {
        patterns[i] = p;
        return i;
    }
    else
        return FOUND;
}

LargePat build_pat(unsigned long key, int id, float prob)
{    
    LargePat pat = {key, id, prob};
    return pat;
}

// Code: ------------------- Zobrist signature computation --------------------
void init_stone_color(void)
{
    memset(color,0,1024);
    color['.'] = 0;                      // 0: EMPTY
    color['#'] = color[' '] = 1;         // 1: OUT  
    color['O'] = color['x'] = 2;         // 2: Other or 'x'
    color['X'] = 3;                      // 3: ours 
}

void init_zobrist_hashdata(void)
{
    for (int d=0 ; d<141 ; d++)  {//d = displacement ...
        for (int c=0 ; c<4 ; c++) {
            unsigned int d1 = qdrandom(), d2=qdrandom();
            unsigned long ld1 = d1, ld2=d2;
            zobrist_hashdata[d][c] = (ld1<<32) + ld2;
        }
    }
}
  
unsigned long zobrist_hash(char *pat) {
    int l = strlen(pat);
    unsigned long k=0;
    for (int i=0 ; i<l ; i++) {
        k ^= zobrist_hashdata[i][color[pat[i]]];
    }
    return k;
}

unsigned long 
update_zobrist_hash_at_point(Point pt, int size, unsigned long k)
// Update the Zobrist signature for the points of pattern of size 'size' 
{
    int imin=pat_gridcular_size[size-1], imax=pat_gridcular_size[size];
    for (int i=imin ; i<imax ; i++) {
        int c = color[large_board[pt+pat_gridcular_seq1d[i]]];
        k ^= zobrist_hashdata[i][c];
    }
    return k;
}

// Code: -------------- rotation and reflexion of the patterns ----------------
void init_gridcular(Shift seq[141], int seq1d[141]) {
    for (int i=0 ; i<141 ; i++)
        seq1d[i] = seq[i].x - seq[i].y*(N+7);
}

int nperms=0;       // current permutation

int permutation_OK(int p[8][141])
{
    for (int i=0 ; i<141 ; i++)
        if (p[0][i] != i) return 0;
    return 1;
}

int gridcular_index(int disp)
{
    for (int i=0 ; i<141 ; i++)
        if (pat_gridcular_seq1d[i] == disp)
            return i;
    log_fmt_s('E', "gridcular_index(): can't happen",NULL);
    return -1;
}

void gridcular_register(Shift seq[141], int p[8][141])
{
    int seq1d[141];
    init_gridcular(seq,seq1d);
    for (int i=0 ; i< 141 ; i++)
        p[nperms][i] = gridcular_index(seq1d[i]);
    nperms++;
}

void gridcular_enumerate2(Shift seq[141], int p[8][141])
{
    Shift seq1[141];
    gridcular_register(seq, p);
    // Horizontal flip of the pattern
    for (int i=0 ; i<141 ; i++) {
        seq1[i].x =  seq[i].x;
        seq1[i].y = -seq[i].y;
    }
    gridcular_register(seq1, p);
}

void gridcular_enumerate1(Shift seq[141], int p[8][141])
{
    Shift seq1[141];
    gridcular_enumerate2(seq, p);
    // Vertical flip of the pattern
    for (int i=0 ; i<141 ; i++) {
        seq1[i].x = -seq[i].x;
        seq1[i].y =  seq[i].y;
    }
    gridcular_enumerate2(seq1, p);
}

void gridcular_enumerate(int p[8][141])
{
    Shift seq1[141];
    gridcular_enumerate1(pat_gridcular_seq, p);
    // 90 deg rotation of the pattern
    for (int i=0 ; i<141 ; i++) {
        seq1[i].x = -pat_gridcular_seq[i].y;
        seq1[i].y =  pat_gridcular_seq[i].x;
    }
    gridcular_enumerate1(seq1, p);
}

void permute(int permutation[8][141],int i,char strpat[256],char strperm[256])
{
    int len = strlen(strpat);
    for (int k=0 ; k<len ; k++)
        strperm[k] = strpat[permutation[i][k]];
    strperm[len] = 0;
}

// Code: -------------------- load pattern definitions ------------------------
void load_prob_file(FILE *f)
{
    float prob;
    int   id,t1,t2;

    while (fgets(buf, 255, f) != NULL) {
        if (buf[0] == '#') continue;
        sscanf(buf,"%f %d %d (s:%d)", &prob, &t1, &t2, &id);
        probs[id] = prob;
    }
}

int load_spat_file(FILE *f)
{
    int  d, id, idmax=-1, len, lenmax=0, npats=0;
    char strpat[256], strperm[256];
    unsigned long k;
    int permutation[8][141];

    // compute the 8 permutations of the gridcular positions corresponding
    // to the 8 possible reflexions or rotations of pattern
    gridcular_enumerate(permutation);

    while (fgets(buf, 255, f) != NULL) {
        if (buf[0] == '#') continue;
        sscanf(buf,"%d %d %s", &id, &d, strpat);
        npats++;
        len = strlen(strpat);
        if (len > lenmax) {
            lenmax = len;
            idmax = id;
        }
        if (id > idmax)
            idmax = id;
        for (int i=0 ; i< 8 ; i++) {
            permute(permutation, i, strpat, strperm);
            assert(permutation_OK(permutation));
            k = zobrist_hash(strperm);
            LargePat pat = build_pat(k, id, probs[id]);
            insert_pat(pat);
        }
    }
    log_fmt_i('I', "read %d patterns", npats);
    log_fmt_i('I', "idmax = %d", idmax);
    sprintf(buf, "pattern length max = %d (found at %d)", lenmax, idmax);
    log_fmt_s('I', buf, NULL);
    large_patterns_loaded = 1;
    return npats;
}

// Code: --------------------------- Large Board ------------------------------
// Large board with border of width 7 (to easily compute neighborhood of points)

void compute_large_coord(void)
// Compute the position in the large board of any point on the board
{
    int lpt, pt;
    for (int y=0 ; y<N ; y++)
       for (int x=0 ; x<N ; x++) {
           pt  = (y+1)*(N+1) + x+1;
           lpt = (y+7)*(N+7) + x+7;
           large_coord[pt] = lpt;
       }
}

void init_large_board(void)
{
    memset(large_board, '#', LARGE_BOARDSIZE);
    compute_large_coord();
}

int large_board_OK(Position *pos)
{
    FORALL_POINTS(pos,pt) {
        if (pos->color[pt] == ' ') continue;
        if (pos->color[pt] != large_board[large_coord[pt]])
            return 0;
    }
    return 1;
}

void print_large_board(FILE *f)
// Print visualization of the current large board position
{
    int k=0;
    fprintf(f,"\n\n");
    for (int row=0 ; row<N+14 ; row++) {
        for (int col=0 ; col<N+7 ; col++,k++) fprintf(f, "%c ", large_board[k]);
        fprintf(f,"\n");
    }
    fprintf(f,"\n\n");
}

void copy_to_large_board(Position *pos)
// Copy the current position to the large board
{
    int lpt=(N+7)*7+7, pt=(N+1)+1;
    for (int y=0 ; y<N ; y++, lpt+=7, pt++)
       for (int x=0 ; x<N ; x++)
           large_board[lpt++] = pos->color[pt++];
    assert(large_board_OK(pos));
}

// Code: ------------------------- Public functions ---------------------------
void init_large_patterns(void)
// Initialize all this stuff
{
    FILE *fspat, *fprob;    // Files containing large patterns

    // Initializations
    init_zobrist_hashdata();
    init_stone_color();
    init_gridcular(pat_gridcular_seq, pat_gridcular_seq1d);
    init_large_board();

    // Load patterns data from files
    patterns = calloc(LENGTH, sizeof(LargePat));
    probs = calloc(1064481, sizeof(float));
    log_fmt_s('I', "Loading pattern probs ...", NULL);
    fprob = fopen("patterns.prob", "r");
    if (fprob == NULL)
        log_fmt_s('w', "Cannot load pattern file:%s","patterns.prob");
    else {
        load_prob_file(fprob);
        fclose(fprob);
    }
    log_fmt_s('I', "Loading pattern spatial dictionary ...", NULL);
    fspat = fopen("patterns.spat", "r");
    if (fspat == NULL)
        log_fmt_s('w', "Warning: Cannot load pattern file:%s","patterns.spat");
    else {
        load_spat_file(fspat);
        fclose(fspat);
    }
    if (fprob == NULL || fspat == NULL) {
        fprintf(stderr, "Warning: michi cannot load pattern files, "
                "It will be much weaker. "
                "Consider lowering EXPAND_VISITS %d->2\n", EXPAND_VISITS);
    }
    log_fmt_s('I', "=========== Hashtable initialization synthesis ==========",
                                                                         NULL);
    // reset the statistics after logging them 
    log_hashtable_synthesis();
    nsearchs = nsuccess = 0;
    sum_len_success=sum_len_failure=0.0;
}

double large_pattern_probability(Point pt)
// return probability of large-scale pattern at coordinate pt. 
// Multiple progressively wider patterns may match a single coordinate,
// we consider the largest one.
{
    double prob=-1.0;
    int matched_len=0, non_matched_len=0;
    unsigned long k=0;

    if (large_patterns_loaded)
        for (int s=1 ; s<13 ; s++) {
            int len = pat_gridcular_size[s];
            k = update_zobrist_hash_at_point(large_coord[pt], s, k);
            int i = find_pat(k);
            if (patterns[i].key==k) {
                prob = patterns[i].prob;
                matched_len = len;
            }
            else if (matched_len < non_matched_len && non_matched_len < len)
                break;
            else
                non_matched_len = len;
        }
    return prob;
}

char* make_list_pat_matching(Point pt, int verbose)
// Build the list of patterns that match at the point pt
{
    unsigned long k=0;
    int i;
    char id[16];

    if (!large_patterns_loaded) return "";

    buf[0] = 0;
    for (int s=1 ; s<13 ; s++) {
        k = update_zobrist_hash_at_point(large_coord[pt], s, k);
        i = find_pat(k); 
        if (patterns[i].key == k) {
            if (verbose) 
                sprintf(id,"%d(%.3f) ", patterns[i].id, patterns[i].prob);
            else
                sprintf(id,"%d ", patterns[i].id);
            strcat(buf, id);
        }
    }
    return buf;
}

void log_hashtable_synthesis() 
{
    double nkeys=0;
    for (int i=0 ; i<LENGTH ; i++) 
        if(patterns[i].key != 0) nkeys +=1.0;
    sprintf(buf,"hashtable entries: %.0lf (fill ratio: %.1lf %%)", nkeys,
                                             100.0 * nkeys / LENGTH);
    log_fmt_s('I', buf, NULL);
    sprintf(buf,"%ld searches, %ld success (%.1lf %%)", nsearchs, nsuccess,
                                        100.0 * (double) nsuccess / nsearchs);
    log_fmt_s('I', buf, NULL);
    sprintf(buf,"average length of searchs -- success: %.1lf, failure: %.1lf",
            sum_len_success/nsuccess, sum_len_failure/(nsearchs-nsuccess));
    log_fmt_s('I', buf, NULL);
}
