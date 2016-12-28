// michi.h -- A minimalistic Go-playing engine (from Petr Baudis michi.py)
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <math.h>
//========================= Definition of Data Structures =====================

// --------------------------- Board Constants --------------------------------
#define N         13
#define W         (N+2)
#define BOARDSIZE ((N+1)*W+1)
#define BOARD_IMIN (N+1)
#define BOARD_IMAX (BOARDSIZE-N-1)
#define LARGE_BOARDSIZE ((N+14)*(N+7))
#define BUFLEN 256
#define MAX_GAME_LEN (N*N*3)
#define SINGLEPT_OK       1
#define SINGLEPT_NOK      0
#define TWOLIBS_TEST      1
#define TWOLIBS_TEST_NO   0
#define TWOLIBS_EDGE_ONLY 1
// ---------------------------- MCTS Constants --------------------------------
#define N_SIMS     1400
#define RAVE_EQUIV 3500
#define EXPAND_VISITS 8
#define PRIOR_EVEN         10   // should be even number; 0.5 prior
#define PRIOR_SELFATARI    10   // negative prior
#define PRIOR_CAPTURE_ONE  15
#define PRIOR_CAPTURE_MANY 30
#define PRIOR_PAT3         10
#define PRIOR_LARGEPATTERN 100  // most moves have relatively small probability
extern  int PRIOR_CFG[];   // priors for moves in cfg dist. 1, 2, 3
#define LEN_PRIOR_CFG      (sizeof(PRIOR_CFG)/sizeof(int))
#define PRIOR_EMPTYAREA    10
#define REPORT_PERIOD  200
#define PROB_HEURISTIC_CAPTURE 0.9   // probability of heuristic suggestions
#define PROB_HEURISTIC_PAT3    0.95  // being taken in playout
#define PROB_SSAREJECT 0.9 // prob of rejecting suggested self-atari in playout
#define PROB_RSAREJECT 0.5 // prob of rejecting random self-atari in playout
                           // this is lower than above to allow nakade
#define RESIGN_THRES     0.2
#define FASTPLAY20_THRES 0.8 //if at 20% playouts winrate is >this, stop reading
#define FASTPLAY5_THRES  0.95 //if at 5% playouts winrate is >this, stop reading

//------------------------------- Data Structures -----------------------------
typedef unsigned char Byte;
typedef unsigned int  Info;
typedef unsigned int  Point;
typedef unsigned long long ZobristHash;
typedef Info* Slist;
typedef enum {PASS_MOVE, RESIGN_MOVE, COMPUTER_BLACK, COMPUTER_WHITE} Code;

typedef struct { // ---------------------- Go Position ------------------------
// Given a board of size NxN (N=9, 19, ...), we represent the position
// as a (N+1)*(N+2)+1 string, with '.' (empty), 'X' (to-play player)
// 'x' (other player), and whitespace (off-board border to make rules
// implementation easier).  Coordinates are just indices in this string.
    char  color[BOARDSIZE];   // string that hold the state of the board
    Byte  env4[BOARDSIZE];    // color encoding for the 4 neighbors
    Byte  env4d[BOARDSIZE];   // color encoding for the 4 diagonal neighbors
    int   n;                  // move number
    Point ko, ko_old;         // position of the ko (0 if no ko)
    Point last, last2, last3; // position of the last move and the move before
    float komi;               // komi for the game
    char  cap;                // number of stones captured by the 'x' player
    char  capX;               // number of stones captured by the 'X' player
} Position;         // Go position

typedef struct tree_node { // ------------ Monte-Carlo tree node --------------
    int v;          // number of visits
    int w;          // number of wins(expected reward is w/v)
    int pv;         // pv, pw are prior values
    int pw;         // (node value = w/v + pw/pv)
    int av;         // av, aw are amaf values ("all moves as first"),
    int aw;         // used for the RAVE tree policy)
    int nchildren;  // number of children
    Position pos;
    struct tree_node **children;
} TreeNode;         //  Monte-Carlo tree node

typedef struct {
    int        value;
    int        in_use;
    int        mark[BOARDSIZE];
} Mark;

// -------------------------------- Global Data -------------------------------
extern Byte bit[8];
extern Byte pat3set[8192];
extern int  npat3;
extern Byte *pat3set_p[15];
extern Mark *already_suggested, *mark1, *mark2;
extern unsigned int idum;
extern FILE         *flog;                     // FILE to log messages
extern int          c1,c2;                     // counters for messages

//================================== Code =====================================
//-------------------------- Functions in debug.c -----------------------------
void log_fmt_i(char type, const char *msg, int n);
void log_fmt_p(char type, const char *msg, Point i);
void log_fmt_s(char type, const char *msg, const char *s);
char* debug(Position *pos);
//-------------------------- Functions in michi.c -----------------------------
void dump_subtree(TreeNode *node,double thres,char *indent,FILE *f,int recurse);
int fix_atari(Position *pos, Point pt, int singlept_ok
        , int twolib_test, int twolib_edgeonly, Slist moves, Slist sizes);
int  gen_playout_moves_capture(Position *pos, Slist heuristic_set, float prob,
                                int expensive_ok, Slist moves, Slist sizes);
int  gen_playout_moves_pat3(Position *pos, Slist heuristic_set, float prob,
                                                                  Slist moves);
void make_list_last_moves_neighbors(Position *pos, Slist points);
double mcplayout(Position *pos, int amaf_map[], int owner_map[], int disp);
Point parse_coord(char *s);
char* play_move(Position *pos, Point pt);
char* pass_move(Position *pos);
void ppoint(Point pt);
void print_pos(Position *pos, FILE *f, int *owner_map);
void print_tree_summary(TreeNode *tree, int sims, FILE *f);
char* slist_str_as_point(Slist l);
char* str_coord(Point pt, char str[5]);
//------------------------- Functions in patterns.c ---------------------------
void   make_pat3set(void);
char*  make_list_pat3_matching(Position *pos, Point pt);
char*  make_list_pat_matching(Point pt, int verbose);
void   init_large_patterns(void);
void   copy_to_large_board(Position *pos);
void   log_hashtable_synthesis();
double large_pattern_probability(Point pt);
//-------------------------- Definition of Useful Macros ----------------------
#ifndef _MSC_VER
    #define __INLINE__ static inline
#else
    #define __INLINE__ static __inline
#endif
#define FORALL_POINTS(pos,i) for(Point i=BOARD_IMIN ; i<BOARD_IMAX ; i++)
#define FORALL_NEIGHBORS(pos, pt, k, n) \
    for(k=0,n=pt+delta[0] ; k<4 ; n=pt+delta[++k])
#define FORALL_DIAGONAL_NEIGHBORS(pos, pt, k, n) \
    for(k=4,n=pt+delta[4] ; k<8 ; n=pt+delta[++k])
#define FORALL_IN_SLIST(l, item) \
    for(int _k=1,_n=l[0],item=l[1] ; _k<=_n ; item=l[++_k])
#define SWAP_CASE(c) {if(c == 'X') c = 'x'; else if (c == 'x') c = 'X'; }
#define SWAP(T, u, v) {T _t = u; u = v; v = _t;}
// Random shuffle: Knuth. The Art of Computer Programming vol.2, 2nd Ed, p.139
#define SHUFFLE(T, l, n) for(int _k=n-1 ; _k>0 ; _k--) {  \
    int _tmp=random_int(_k); SWAP(T, l[_k], l[_tmp]); \
}

//------------ Useful utility functions (inlined for performance) -------------
// Quick and Dirty random generator (32 bits Linear Congruential Generator)
// Ref: Numerical Recipes in C (W.H. Press & al), 2nd Ed page 284
__INLINE__ unsigned int qdrandom(void) {idum=(1664525*idum)+1013904223; return idum;}
__INLINE__ unsigned int random_int(int n) /* random int between 0 and n-1 */ \
           {unsigned long long r=qdrandom(); return (r*n)>>32;}

// Go programs manipulates lists or sets of (small) integers a lot. There are
// many possible implementations that differ by the performance of the various
// operations we need to perform on these data structures.
// insert, remove, enumerate elements, test "is in ?", make the set empty ...
//
// Here are 2 simple implementations of sets
// * Slist is a simple list implementation as integer arrays (not sorted)
//   - it is slow for finding or inserting without dupplicate except for very
//     small sets.
//   - but fast to enumerate elements or insert when there is no need to check
//     for dupplicates
// * Mark
//   - it is very fast for insert, remove elements, making the set empty or to
//     test "is in ?"
//   - but slow if we need to enumerate all the elements
// These two representations are necessary for performance even with the goal
// of brevity that is a priority for michi (actually the code is very short).
//
// Note: the pat3_set in patterns.c is a representation of a set as bitfield.

__INLINE__ int  slist_size(Slist l) {return l[0];}
__INLINE__ void slist_clear(Slist l) {l[0]=0;}
__INLINE__ void slist_push(Slist l, Info item) {l[l[0]+1]=item;l[0]++;}
__INLINE__ void slist_range(Slist l,int n)
        {l[0]=n; for (int k=0;k<n;k++) l[k+1]=k;}
__INLINE__ void slist_shuffle(Slist l) { Info *t=l+1; SHUFFLE(Info,t,l[0]); }

__INLINE__ int  slist_insert(Slist l, Info item)
{
    int k, n=l[0] + 1;
    l[n] = item;
    for (k=1 ; k<=n ; k++)
        if(l[k] == item) break;
    if(k == n) {
        l[0] = n; return 1;   // return 1 if the item has been inserted
    }
    else return 0;
}
__INLINE__ void slist_append(Slist dest, Slist src)
{
    FORALL_IN_SLIST(src, i)
        slist_push(dest, i);
}

// Quick marker : idea borrowed from the Gnugo codebase
__INLINE__ void mark_init(Mark *m) {m->in_use = 1; m->value++;}
__INLINE__ void mark_release(Mark *m) {m->in_use = 0;}   // for debugging
__INLINE__ void mark(Mark *m, Info i) {m->mark[i] = m->value;}
__INLINE__ int  is_marked(Mark *m, Info i) {return m->mark[i] == m->value;}

// Pattern matching
__INLINE__ int pat3_match(Position *pos, Point pt)
{
    int env8=(pos->env4d[pt] << 8) + pos->env4[pt], q=env8 >> 3, r=env8 & 7;
    return (pat3set[q] & bit[r]) != 0;
}
