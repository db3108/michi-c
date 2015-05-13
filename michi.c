// michi.c -- A minimalistic Go-playing engine
/*
This is a recoding in C (for speed) of the michi.py code by Petr Baudis
avalaible at https://github.com/pasky/michi .

(c) 2015 Petr Baudis <pasky@ucw.cz> Denis Blumstein <db3108@free.fr>
MIT licence (i.e. almost public domain)

The following comments are taken almost verbatim from the michi.py code

A minimalistic Go-playing engine attempting to strike a balance between
brevity, educational value and strength.  It can beat GNUGo on 13x13 board
on a modest 4-thread laptop.

To start reading the code, begin either:
* Bottom up, by looking at the goban implementation - starting with
  the 'empty_position' definition below and play_move() function.
* In the middle, by looking at the Monte Carlo playout implementation,
  starting with the mcplayout() function.
* Top down, by looking at the MCTS implementation, starting with the
  tree_search() function. It is just a loop of tree_descend(),
  mcplayout() and tree_update() round and round.

It may be better to jump around a bit instead of just reading straight
from start to end.

The C code can be read in parallel with the python code. 
I have been careful to keep the notations used by Petr (almost) everywhere.
Of course the algorithms are the same (at least functionally) as well as the
parameters. 

Examples where the python and the C codes are different are:
- in the functions gen_playout_moves_xxx(). I have not been able to emulate in 
  C the generators that are available in python (yield instruction). So these
  functions in the C code must compute the whole list of suggestions before 
  returning.
- computation of blocks does not use regexp as the direct coding is simple.
- need to recode a functionality equivalent to python dictionary (in patterns.c)

The source is composed in 7 independent parts
- Utilities
- Board routines
- Go heuristics
- Monte Carlo Playout policy
- Monte Carlo Tree search
- User Interface (Utilities, Various main programs)
- Pattern code (3x3 and large patterns) which is found in patterns.c

In C, functions prototypes must be declared before use. 
In order to avoid these declarations, functions are defined before they are 
used, which has the same effect.
This means that the higher level functions are found towards the bottom of
this file. This may not be a good idea in terms of readibility but at least
the order is the same as in the michi python code.

Short bibliography
------------------
[1] Martin Mueller, Computer Go, Artificial Intelligence, Vol.134, No 1-2,
    pp 145-179, 2002
[2] Remi Coulom.  Efficient Selectivity and Backup Operators in Monte-Carlo Tree
    Search.  Paolo Ciancarini and H. Jaap van den Herik.  5th International 
    Conference on Computer and Games, May 2006, Turin, Italy.  2006. 
    <inria-00116992>
[3] Sylvain Gelly, Yizao Wang, Remi Munos, Olivier Teytaud.  Modification of UCT
    with Patterns in Monte-Carlo Go. [Research Report] RR-6062, 2006.
    <inria-00117266v3>
[4] David Stern, Ralf Herbrich, Thore Graepel, Bayesian Pattern Ranking for Move
    Prediction in the Game of Go, In Proceedings of the 23rd international 
    conference on Machine learning, pages 873–880, Pittsburgh, Pennsylvania, 
    USA, 2006
[5] Rémi Coulom. Computing Elo Ratings of Move Patterns in the Game of Go. 
    In ICGA Journal (2007), pp 198-208.
[6] Sylvain Gelly, David Silver. Achieving Master Level Play in 9×9 Computer Go.
    Proceedings of the Twenty-Third AAAI Conference on Artificial Intelligence 
    (2008)
[7] Albert L Zobrist. A New Hashing Method with Application for Game Playing.
    Technical Report #88. April 1970
[8] Petr Baudis. MCTS with Information Sharing, PhD Thesis, 2011
[9] Robert Sedgewick, Algorithms in C, Addison-Wesley, 1990

+ many other PhD thesis accessible on the WEB

[1] can be consulted for the definition of Computer Go terms : 
    points, blocks, eyes, false eyes, liberties, etc.
    and historical bibliography
*/

#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <unistd.h>
#include "michi.h"

void usage() {
    fprintf(stderr, "\n\nusage: michi [-z SEED] [command]\n\n"
                    "where  command = gtp|mcdebug|mcbenchmark|tsdebug\n"
                    "       SEED    = > 0 (fixed seed) or 0 (random seed)\n");
    exit(-1);
}

//========================= Definition of Data Structures =====================
// Given a board of size NxN (N=9, 19, ...), we represent the position
// as an (N+1)*(N+2)+1 string, with '.' (empty), 'X' (to-play player)
// 'x' (other player), and whitespace (off-board border to make rules
// implementation easier).  Coordinates are just indices in this string.
//
// -------------------------------- Global Data -------------------------------
//                      North East South  West  NE  SE  SW  NW
static int   delta[] = { -N-1,   1,  N+1,   -1, -N,  W,  N, -W};
static char* colstr  = "@ABCDEFGHJKLMNOPQRST";
Mark         *mark1, *mark2, *already_suggested;
unsigned int idum=1;
char         buf[BUFLEN];
Point        allpoints[BOARDSIZE];
int          PRIOR_CFG[] =     {24, 22, 8};
Point        pos_capture;

//================================== Code =====================================
// Utilities
char* slist_str_as_int(Slist l) {
    buf[0]=0;
    for (int k=1, n=l[0] ; k<=n ; k++) {
        char s[32];
        sprintf(s, " %d", l[k]);
        strcat(buf, s);
    }
    return buf;
}
char* slist_str_as_point(Slist l) {
    buf[0]=0;
    for (int k=1, n=l[0] ; k<=n ; k++) {
        char str[8], s[8];
        sprintf(s, " %s", str_coord(l[k],str));
        strcat(buf, s);
    }
    return buf;
}

uint
true_random_seed(void)
// return a true random seed (which depends on the time)
{
    uint r1, r2, sec, usec ;
    struct timeval tp;
    gettimeofday(&tp, NULL);
    usec = tp.tv_usec;
    sec  = tp.tv_sec;
    // Park & Miller random generator (same as qdrandom())
    r1 =  (1664525*sec) + 1013904223;
    r2 = (1664525*usec) + 1013904223;
    return (r1^r2);
}

//=============================== Board routines ==============================
char is_eyeish(Position *pos, Point pt)
// test if pt is inside a single-color diamond and return the diamond color or 0
// this could be an eye, but also a false one
{
    char eyecolor=0, othercolor;
    int k;
    Point n;
    FORALL_NEIGHBORS(pos, pt, k, n) {
        char c = pos->color[n];
        if(c == ' ') continue;                // ignore OUT of board neighbours
        if(c == '.') return 0;
        if(eyecolor == 0) {
            eyecolor = c;
            othercolor = c; SWAP_CASE(othercolor);
        }
        else if (c == othercolor) return 0;
    }
    return eyecolor;
}

char is_eye(Position *pos, Point pt)
// test if pt is an eye and return its color or 0.
//                                                  #########   
// Note: this test cannot detect true eyes like     . . X . #   or    X X X
//                                                    X . X #         X   X
//                                                    X X . #           X   X
//                                                        . #           X X X
{
    char eyecolor=is_eyeish(pos, pt), falsecolor=eyecolor;
    int at_edge=0, false_count=0, k;
    Point d;
    if (eyecolor == 0) return 0;
    
    // Eye-like shape, but it could be a falsified eye
    SWAP_CASE(falsecolor);
    FORALL_DIAGONAL_NEIGHBORS(pos, pt, k, d) {
        if(pos->color[d] == ' ') at_edge = 1;
        else if(pos->color[d] == falsecolor) false_count += 1;
    }
    if (at_edge) false_count += 1;
    if (false_count >= 2) return 0;
    return eyecolor;
}

Byte compute_env4(Position *pos, Point pt, int offset)
// Compute value of the environnement of a point (Byte)
// offset=0 for the 4 neighbors, offset=4 for the 4 diagonal neighbors
{
    Byte env4=0, hi, lo, c;
    for (int k=offset ; k<offset+4 ; k++) {
        Point n = pt + delta[k];
        // color coding c -> 0:WHITE, 1:BLACK, 2:EMPTY, 3:OUT 
        if (pos->color[n] == '.')         c = 2;
        else if(pos->color[n] == ' ')     c = 3;
        else {
            // env4 is computed with real colors on the board
            if (pos->n%2==0) {      // BLACK to play (X=BLACK, x=WHITE)
                if (pos->color[n] == 'X') c = 1;
                else                      c = 0;
            }
            else {                  // WHITE to play (X=WHITE, x=BLACK)
                if (pos->color[n] == 'X') c = 0;
                else                      c = 1;
            }
        }
        hi = c >> 1; lo = c & 1;
        env4 |= ((hi<<4)+lo) << (k-offset);
    }
    return env4;
}

void put_stone(Position *pos, Point pt)
// Always put a stone of color 'X'. See discussion on env4 in patterns.c
{
    if (pos->n%2 == 0) {  // BLACK to play (X=BLACK)
        pos->env4[pt+N+1] ^= 0x11;
        pos->env4[pt-1]   ^= 0x22;
        pos->env4[pt-N-1] ^= 0x44;
        pos->env4[pt+1]   ^= 0x88;
        pos->env4d[pt+N]  ^= 0x11;
        pos->env4d[pt-W]  ^= 0x22;
        pos->env4d[pt-N]  ^= 0x44;
        pos->env4d[pt+W]  ^= 0x88;
    }
    else {                // WHITE to play (X=WHITE)
        pos->env4[pt+N+1] &= 0xEE;
        pos->env4[pt-1]   &= 0xDD;
        pos->env4[pt-N-1] &= 0xBB;
        pos->env4[pt+1]   &= 0x77;
        pos->env4d[pt+N]  &= 0xEE;
        pos->env4d[pt-W]  &= 0xDD;
        pos->env4d[pt-N]  &= 0xBB;
        pos->env4d[pt+W]  &= 0x77;
    }
    pos->color[pt] = 'X';
}

void remove_stone(Position *pos, Point pt)
// Always remove a stone of color 'x' (cheat done by caller when undo move)
{
    if (pos->n%2 == 0) {  // BLACK to play (x=WHITE)
        pos->env4[pt+N+1] |= 0x10;
        pos->env4[pt-1]   |= 0x20;
        pos->env4[pt-N-1] |= 0x40;
        pos->env4[pt+1]   |= 0x80;
        pos->env4d[pt+N]  |= 0x10;
        pos->env4d[pt-W]  |= 0x20;
        pos->env4d[pt-N]  |= 0x40;
        pos->env4d[pt+W]  |= 0x80;
    }
    else {                // WHITE to play (x=BLACK)
        pos->env4[pt+N+1] ^= 0x11;
        pos->env4[pt-1]   ^= 0x22;
        pos->env4[pt-N-1] ^= 0x44;
        pos->env4[pt+1]   ^= 0x88;
        pos->env4d[pt+N]  ^= 0x11;
        pos->env4d[pt-W]  ^= 0x22;
        pos->env4d[pt-N]  ^= 0x44;
        pos->env4d[pt+W]  ^= 0x88;
    }
    pos->color[pt] = '.';
}

void dump_env4(Byte env4, Byte true_env4)
{
    for (int i=0 ; i<8 ; i++) {
        if (i == 4) fprintf(stderr, " ");
        if (env4 & 128)
            fprintf(stderr, "1");
        else
            fprintf(stderr, "0");
        env4 <<= 1;
    }
    fprintf(stderr, " (true: ");
    for (int i=0 ; i<8 ; i++) {
        if (i==4) fprintf(stderr, " ");
        if (true_env4 & 128)
            fprintf(stderr, "1");
        else
            fprintf(stderr, "0");
        true_env4 <<= 1;
    }
    fprintf(stderr, ")\n");
}

int env4_OK(Position *pos)
{
    FORALL_POINTS(pos,pt) {
        if (pos->color[pt] == ' ') continue;
        if (pos->env4[pt] != compute_env4(pos, pt, 0)) {
            fprintf(stderr, "%s ERR env4 = ", str_coord(pt,buf));
            dump_env4(pos->env4[pt], compute_env4(pos,pt,0));
            return 0;
        }
        if (pos->env4d[pt] != compute_env4(pos,pt,4)) {
            fprintf(stderr, "%s ERR env4d = ", str_coord(pt,buf));
            dump_env4(pos->env4d[pt], compute_env4(pos,pt,4));
            return 0;
        }
    }
    return 1;
}

char* empty_position(Position *pos)
// Reset pos to an initial board position
{
    int k = 0;
    for (int col=0 ; col<=N ; col++) pos->color[k++] = ' ';
    for (int row=1 ; row<=N ; row++) {
        pos->color[k++] = ' ';
        for (int col=1 ; col<=N ; col++) pos->color[k++] = '.';
    }
    for (int col=0 ; col<W ; col++) pos->color[k++] = ' ';
    FORALL_POINTS(pos, pt) {
        if (pos->color[pt] == ' ') continue;
        pos->env4[pt] = compute_env4(pos, pt, 0);
        pos->env4d[pt] = compute_env4(pos, pt, 4);
    }

    pos->ko = pos->last = pos->last2 = 0;
    pos->capX = pos->cap = 0;
    pos->n = 0; pos->komi = 7.5;
    assert(env4_OK(pos));
    return "";              // result OK
}

void compute_block(Position *pos, Point pt, Slist stones, Slist libs, int nlibs)
// Compute block at pt : list of stones and list of liberties
// Return early when nlibs liberties are found
{
    char  color=pos->color[pt];
    int   head=2, k, tail=1;
    Point n;

    mark_init(mark1); slist_clear(libs);
    stones[1] = pt; mark(mark1, pt);
    while(head>tail) {
        pt = stones[tail++];
        FORALL_NEIGHBORS(pos, pt, k, n)
            if (!is_marked(mark1, n)) {
                mark(mark1, n);
                if (pos->color[n] == color)    stones[head++] = n;
                else if (pos->color[n] == '.') {
                    slist_push(libs, n);
                    if (slist_size(libs) >= nlibs) goto finished;
                }
            }
    }
finished:
    stones[0] = head-1;
    mark_release(mark1);
}

int capture_block(Position *pos, Slist stones)
{
    FORALL_IN_SLIST(stones, pt) remove_stone(pos, pt);
    assert(env4_OK(pos));
    return slist_size(stones);
}

void swap_color(Position *pos) 
{
    FORALL_POINTS(pos, pt)
        SWAP_CASE(pos->color[pt]);
}

void remove_X_stone(Position *pos, Point pt)
{
    (pos->n)++;             // cheat to make remove_stone() work
    remove_stone(pos, pt);
    (pos->n)--;             // undo cheat
}

void undo_move(Position *pos)
// WARNINGS: can only undo one move, can only undo capture of 1 stone
// Enough to undo snap back
{
    remove_stone(pos, pos->last);
    pos->last = pos->last2; pos->last2 = pos->last3;
    pos->ko = pos->ko_old;
    if (pos_capture) {
        put_stone(pos, pos_capture);
        pos->cap -=1;
    }
    (pos->n)--;
    SWAP(char,pos->cap, pos->capX);
    swap_color(pos);
    assert(env4_OK(pos));
}

char* play_move(Position *pos, Point pt)
// Play a move at point pt (color is imposed by alternate play)
{
    int   captured=0, k;
    Point libs[BOARDSIZE], n, stones[BOARDSIZE];

    pos->ko_old = pos->ko;
    if (pt == pos->ko) return "Error Illegal move: retakes ko";
    int in_enemy_eye = is_eyeish(pos, pt);

    put_stone(pos, pt);
    // Check for captures
    pos_capture = 0;
    FORALL_NEIGHBORS(pos, pt, k, n) {
        if (pos->color[n] != 'x') continue;
        compute_block(pos,n,stones, libs, 1);                // extremely naive
        if (slist_size(libs)==0) {
            captured += capture_block(pos, stones);
            pos_capture = n;
        }

    }
    if (captured) { // Set ko
        if (captured==1 && in_enemy_eye) pos->ko = pos_capture;
        else                             pos->ko = 0;
    }
    else { // Test for suicide
        pos->ko = 0;
        compute_block(pos, pt, stones, libs, 1);
        if(slist_size(libs) == 0) {
            pos->ko = pos->ko_old;
            remove_X_stone(pos, pt);
            return "Error Illegal move: suicide";
        }
    }
    // Finish update of the position
    captured += pos->capX;
    pos->capX = pos->cap;
    pos->cap  = captured;
    swap_color(pos);
    (pos->n)++;
    assert(env4_OK(pos));
    pos->last3 = pos->last2;
    pos->last2 = pos->last;
    pos->last  = pt;
    return "";          // Move OK
}

char* pass_move(Position *pos)
// Pass - i.e. simply flip the position
{
    swap_color(pos); (pos->n)++;
    pos->last2 = pos->last;
    pos->last  = pos->ko = 0;
    SWAP(int,pos->cap, pos->capX);
    return "";          // PASS moVE is always OK
}

void make_list_neighbors(Position *pos, Point pt, Slist points)
{
    slist_clear(points);
    if (pt == PASS_MOVE) return;
    slist_push(points, pt);
    for (int k=0 ; k<8 ; k++)
        if (pos->color[pt+delta[k]] != ' ')
            slist_push(points, pt+delta[k]);
    slist_shuffle(points);
}

void make_list_last_moves_neighbors(Position *pos, Slist points)
// generate a randomly shuffled list of points including and surrounding 
// the last two moves (but with the last move having priority)
{
    Point last2_neighbors[12];
    make_list_neighbors(pos, pos->last,points);
    make_list_neighbors(pos, pos->last2,last2_neighbors);
    FORALL_IN_SLIST(last2_neighbors, n)
        slist_insert(points, n);     // insert n if it is not already in points
}

void make_list_neighbor_blocks_in_atari(Position *pos, Slist stones, 
        Slist breps, Slist libs)
// Return a list of (opponent) blocks in contact with point in stones
// Each block in the list is represented by one of its points brep
{
    char  color = pos->color[stones[1]];
    int   k, maxlibs=2;
    Point n, st[BOARDSIZE], l[4];

    if (color == 'x') color = 'X';
    else              color = 'x';

    mark_init(mark2); slist_clear(breps); slist_clear(libs);
    FORALL_IN_SLIST(stones, pt) {
        FORALL_NEIGHBORS(pos, pt, k, n) {
            if (pos->color[n] == color && !is_marked(mark2, n)) {
                compute_block(pos, n, st, l, maxlibs);
                if (slist_size(l) == 1) {
                    slist_push(breps, st[1]);
                    slist_push(libs, l[1]);
                    FORALL_IN_SLIST(st, p)
                        mark(mark2, p);
                }
            }
        }
    }
    mark_release(mark2);
}

double score(Position *pos, int owner_map[])
// compute score for to-play player; this assumes a final position with all 
// dead stones captured and only single point eyes on the board ...
{
    double s=pos->komi;
    int    n=-1;
    if (pos->n%2==0) {
        s = -s;           // komi counts negatively for BLACK
        n = 1;
    }

    FORALL_POINTS(pos,pt) {
        char c = pos->color[pt];
        if (c=='.') c = is_eyeish(pos,pt);
        if (c=='X') {
            s += 1.0;
            owner_map[pt] += n;
        }
        else if (c=='x') {
            s -= 1.0;
            owner_map[pt] -= n;
        }
    }
    return s;
}

//================================ Go heuristics ==============================
// The couple of functions read_ladder_attack / fix_atari is maybe the most 
// complicated part of the whole program (sadly). 
// Feel free to just TREAT IT AS A BLACK-BOX, it's not really that interesting!

Point read_ladder_attack(Position *pos, Point pt, Slist libs)
// Check if a capturable ladder is being pulled out at pt and return a move
// that continues it in that case. Expects its two liberties in libs.
// Actually, this is a general 2-lib capture exhaustive solver.
{
    Point moves[5], sizes[5];   // 4 points should be enough ...
    Point move=0, pos_capture_old = pos_capture;
    FORALL_IN_SLIST(libs, l) {
        Position pos_l = *pos;
        char *ret = play_move(&pos_l, l);
        if (ret[0]!=0) continue; // move not legal
        // fix_atari() will recursively call read_ladder_attack() back
        // however, ignore 2lib groups as we don't have time to chase them
        slist_clear(moves); slist_clear(sizes);
        int is_atari = fix_atari(&pos_l, pt, SINGLEPT_NOK, TWOLIBS_TEST_NO
                                                           , 0, moves, sizes);
        // if block is in atari and cannot escape, it is caugth in a ladder
        if (is_atari && slist_size(moves) == 0) 
            move = l; 
    }
    pos_capture = pos_capture_old;
    return move;   // ladder attack not successful
}

int line_height(Point pt);
int fix_atari(Position *pos, Point pt, int singlept_ok
        , int twolib_test, int twolib_edgeonly, Slist moves, Slist sizes)
// An atari/capture analysis routine that checks the group at Point pt,
// determining whether (i) it is in atari (ii) if it can escape it,
// either by playing on its liberty or counter-capturing another group.
//
// Return 1 (true) if the group is in atari, 0 otherwise
//        moves : a list of moves that capture or save blocks
//        sizes : list of same lenght as moves (size of corresponding blocks)
// singlept_ok!=0 means that we will not try to save one-point groups
{
    int in_atari=1, maxlibs=3;
    Point stones[BOARDSIZE], l, libs[5], blocks[256], blibs[256];

    slist_clear(moves); slist_clear(sizes);
    compute_block(pos, pt, stones, libs, maxlibs);
    if (singlept_ok && slist_size(stones) == 1) return 0;
    if (slist_size(libs) >= 2) { 
        if (twolib_test && slist_size(libs) == 2 && slist_size(stones) > 1) {
            if (twolib_edgeonly
                   && ((line_height(libs[1]))>0 || (line_height(libs[2]))>0)) {
                // no expensive ladder check
                return 0;
            }
            else {
                // check that the block cannot be caught in a working ladder
                // If it can, that's as good as in atari, a capture threat.
                // (Almost - N/A for countercaptures.)
                Point ladder_attack = read_ladder_attack(pos, pt, libs);
                if (ladder_attack) {
                    if(slist_insert(moves, ladder_attack))
                        slist_push(sizes, slist_size(stones));
                }
            }
        } 
        return 0;  
    }

    if (pos->color[pt] == 'x') { 
        // - this is opponent's group, that's enough to capture it
        if (slist_insert(moves, libs[1])) 
            slist_push(sizes, slist_size(stones));
        return in_atari;
    }

    // This is our group and it is in atari
    // Before thinking about defense, what about counter-capturing a neighbor ?
    make_list_neighbor_blocks_in_atari(pos, stones, blocks, blibs);
    FORALL_IN_SLIST(blibs, l)
        if (slist_insert(moves, l))
            slist_push(sizes, slist_size(stones));

    l = libs[1];
    // We are escaping.  
    // Will playing our last liberty gain/ at least two liberties?
    Point pos_capture_old = pos_capture;
    Position escpos = *pos;
    char *ret = play_move(&escpos, l);
    if (ret[0]!=0)
        return 1;     // oops, suicidal move
    pos_capture = pos_capture_old;
    compute_block(&escpos, l, stones, libs, maxlibs);  
    if (slist_size(libs) >= 2) {
        // Good, there is still some liberty remaining - but if it's just the 
        // two, check that we are not caught in a ladder... (Except that we 
        // don't care if we already have some alternative escape routes!)
        if (slist_size(moves)>1 
        || (slist_size(libs)==2 && read_ladder_attack(&escpos,l,libs) == 0)
        || (slist_size(libs)>=3))
            if (slist_insert(moves, l))
                slist_push(sizes, slist_size(stones));
    }
    return in_atari;
}

void compute_cfg_distances(Position *pos, Point pt, char cfg_map[BOARDSIZE])
// Return a board map listing common fate graph distances from a given point.
// This corresponds to the concept of locality while contracting groups to 
// single points.
{
    int   head=1, k, tail=0;
    Point fringe[30*BOARDSIZE], n;

    memset(cfg_map, -1, BOARDSIZE);
    cfg_map[pt] = 0;

    // flood-fill like mechanics
    fringe[0]=pt;
    while(head > tail) {
        pt = fringe[tail++];
        FORALL_NEIGHBORS(pos, pt, k, n) {
            char c = pos->color[n];
            if (c==' ') continue;
            if (0 <= cfg_map[n] && cfg_map[n] <= cfg_map[pt]) continue;
            int cfg_before = cfg_map[n];
            if (c != '.' && c==pos->color[pt]) 
                cfg_map[n] = cfg_map[pt];
            else
                cfg_map[n] = cfg_map[pt]+1;
            if (cfg_before < 0 || cfg_before > cfg_map[n]) {
                fringe[head++] = n;
                assert(head < 30*BOARDSIZE);
            }
        }
    }
}

int line_height(Point pt)
// Return the line number above nearest board edge (0 based)
{
    div_t d = div(pt,N+1);
    int row = d.quot, col=d.rem;
    if (row > N/2) row = N+1-row;
    if (col > N/2) col = N+1-col;
    if (row < col) return row-1;
    else           return col-1;
}

int empty_area(Position *pos, Point pt, int dist)
// Check whether there are any stones in Manhattan distance up to dist
{
    int   k;
    Point n;
    FORALL_NEIGHBORS(pos, pt, k, n) {
        if (pos->color[n]=='x' || pos->color[n]=='X') 
            return 0;
        else if (pos->color[n]=='.' && dist>1 && !empty_area(pos, n, dist-1))
            return 0;
    }
    return 1;
}

//========================= Montecarlo playout policy =========================
int gen_playout_moves_capture(Position *pos, Slist heuristic_set, float prob,
                                    int expensive_ok, Slist moves, Slist sizes)
// Compute list of candidate next moves in the order of preference (capture)
// heuristic_set is the set of coordinates considered for applying heuristics;
// this is the immediate neighborhood of last two moves in the playout, but
// the whole board while prioring the tree.
{
    int   k, twolib_edgeonly = !expensive_ok;
    Point move2[20], size2[20];

    slist_clear(moves); slist_clear(sizes);
    FORALL_IN_SLIST(heuristic_set, pt)
        if (pos->color[pt]=='x' || pos->color[pt]=='X') {
            fix_atari(pos, pt, SINGLEPT_NOK, TWOLIBS_TEST,
                                            twolib_edgeonly, move2, size2);
            k=1;
            FORALL_IN_SLIST(move2, move)
                if (slist_insert(moves, move))
                    slist_push(sizes, size2[k++]);
        }
    return slist_size(moves);
}

int gen_playout_moves_pat3(Position *pos, Slist heuristic_set, float prob,
                                                                Slist moves)
// Compute list of candidate next moves in the order of preference (3x3 pattern)
// heuristic_set is the set of coordinates considered for applying heuristics;
// this is the immediate neighborhood of last two moves in the playout, but
// the whole board while prioring the tree.
{
    slist_clear(moves);
    mark_init(already_suggested);
    if (random_int(1000) <= prob*1000.0)
        FORALL_IN_SLIST(heuristic_set, pt)
            if (pos->color[pt] == '.' && pat3_match(pos, pt))
               slist_push(moves, pt); 
    mark_release(already_suggested);
    return slist_size(moves);
}

int gen_playout_moves_random(Position *pos, Point moves[BOARDSIZE], Point i0)
// Generate a list of moves (includes false positives - suicide moves;
// does not include true-eye-filling moves), starting from a given board index
// (that can be used for randomization)
{
    slist_clear(moves);
    for(Point i=i0 ; i<BOARD_IMAX ; i++) {
        if (pos->color[i] != '.') continue;    // ignore NOT EMPTY Points
        if (is_eye(pos,i) == 'X') continue;    // ignore true eyes for player
        slist_push(moves, i); 
    }
    for(Point i=BOARD_IMIN-1 ; i<i0 ; i++) {
        if (pos->color[i] != '.') continue;    // ignore NOT EMPTY Points
        if (is_eye(pos,i) == 'X') continue;    // ignore true eyes for player
        slist_push(moves, i); 
    }
    return slist_size(moves);
}

Point choose_from(Position *pos, Slist moves, char *kind, int disp)
{
    char   *ret;
    Info   sizes[20];
    Point  move = PASS_MOVE, ds[20];

    FORALL_IN_SLIST(moves, pt) {
        if (disp && strcmp(kind, "random")!=0)
            fprintf(stderr,"move suggestion (%s) %s\n", kind,str_coord(pt,buf));
        ret = play_move(pos, pt);
        if (ret[0] == 0) {    // move OK
            move = pt;
            // check if the suggested move did not turn out to be a self-atari
            int r = random_int(10000), tstrej;
            if (strcmp(kind,"random") == 0) tstrej = r<=10000.0*PROB_RSAREJECT;
            else                            tstrej = r<= 10000.0*PROB_SSAREJECT;
            if (tstrej) {
                slist_clear(ds); slist_clear(sizes);
                fix_atari(pos, pt, SINGLEPT_OK, TWOLIBS_TEST, 1, ds, sizes);
                if (slist_size(ds) > 0) {
                    if(disp) fprintf(stderr, "rejecting self-atari move %s\n",
                                                           str_coord(pt, buf));
                    undo_move(pos);
                    move = PASS_MOVE;
                    continue;
                }
            }
            break;
        }
    }
    return move;
}

double mcplayout(Position *pos, int amaf_map[], int owner_map[], int disp)
// Start a Monte Carlo playout from a given position, return score for to-play
// player at the starting position; amaf_map is board-sized scratchpad recording// who played at a given position first
{
    double s=0.0;
    int    passes=0, start_n=pos->n;
    Info   sizes[BOARDSIZE];
    Point  last_moves_neighbors[20], moves[BOARDSIZE], move;
    if(disp) fprintf(stderr, "** SIMULATION **\n");

    while (passes < 2 && pos->n < MAX_GAME_LEN) {
        move = 0;
        if(disp) print_pos(pos, stdout, NULL);
        // We simply try the moves our heuristics generate, in a particular
        // order, but not with 100% probability; this is on the border between
        // "rule-based playouts" and "probability distribution playouts".
        make_list_last_moves_neighbors(pos, last_moves_neighbors);

        // Capture heuristic suggestions
        if (gen_playout_moves_capture(pos, last_moves_neighbors,
                               PROB_HEURISTIC_CAPTURE, 0, moves, sizes))
            if((move=choose_from(pos, moves, "capture", disp)) != PASS_MOVE)
                goto found;

        // 3x3 patterns heuristic suggestions
        if (gen_playout_moves_pat3(pos, last_moves_neighbors,
                                           PROB_HEURISTIC_PAT3, moves))
            if((move=choose_from(pos, moves, "pat3", disp)) != PASS_MOVE) 
                goto found;
            
        gen_playout_moves_random(pos, moves, BOARD_IMIN-1+random_int(N*W));
        move=choose_from(pos, moves, "random", disp);
found:
        if (move == PASS_MOVE) {      // No valid move : pass
            pass_move(pos);
            passes++;
        }
        else {
            if (amaf_map[move] == 0)      // mark the point with 1 for BLACK
                // pos->n-1 because in michi.py pos is updated after this line
                amaf_map[move] = ((pos->n-1)%2==0 ? 1 : -1);
            passes=0;
        }
    }
    s = score(pos, owner_map);
    if (start_n%2 != pos->n%2) s = -s;
    return s;
}
//========================== Montecarlo tree search ===========================
TreeNode* new_tree_node(Position *pos)
{
    TreeNode *node = calloc(1,sizeof(TreeNode));
    node->pos = *pos;
    node->pv = PRIOR_EVEN; node->pw = PRIOR_EVEN/2;
    return node;
}

void expand(TreeNode *tree)
// add and initialize children to a leaf node
{
    char     cfg_map[BOARDSIZE];
    int      nchildren = 0;
    Info     sizes[BOARDSIZE];
    Point    moves[BOARDSIZE], pos_capture_old=pos_capture;
    Position pos2;
    TreeNode *childset[BOARDSIZE], *node;
    if (tree->pos.last!=PASS_MOVE)
        compute_cfg_distances(&tree->pos, tree->pos.last, cfg_map);

    // Use light random playout generator to get all the empty points (not eye)
    gen_playout_moves_random(&tree->pos, moves, BOARD_IMIN-1);

    tree->children = calloc(slist_size(moves)+1, sizeof(TreeNode*));
    FORALL_IN_SLIST(moves, pt) {
        pos2 = tree->pos;
        assert(tree->pos.color[pt] == '.');
        char* ret = play_move(&pos2, pt);
        if (ret[0] != 0) continue;
        pos_capture = pos_capture_old;
        // pt is a legal move : we build a new node for it
        childset[pt]= tree->children[nchildren++] = new_tree_node(&pos2);
    }
    tree->nchildren = nchildren;

    // Update the prior for the 'capture' and 3x3 patterns suggestions
    gen_playout_moves_capture(&tree->pos, allpoints, 1, 1, moves, sizes);
    int k=1;
    FORALL_IN_SLIST(moves, pt) {
        pos2 = tree->pos;
        char* ret = play_move(&pos2, pt);
        if (ret[0] != 0) continue;
        pos_capture = pos_capture_old;
        node = childset[pt];
        if (sizes[k] == 1) {
            node->pv += PRIOR_CAPTURE_ONE;
            node->pw += PRIOR_CAPTURE_ONE;
        }
        else {
            node->pv += PRIOR_CAPTURE_MANY;
            node->pw += PRIOR_CAPTURE_MANY;
        }
        k++;
    }
    gen_playout_moves_pat3(&tree->pos, allpoints, 1, moves);
    FORALL_IN_SLIST(moves, pt) {
        pos2 = tree->pos;
        char* ret = play_move(&pos2, pt);
        if (ret[0] != 0) continue;
        pos_capture = pos_capture_old;
        node = childset[pt];
        node->pv += PRIOR_PAT3;
        node->pw += PRIOR_PAT3;
    }

    // Second pass setting priors, considering each move just once now
    copy_to_large_board(&tree->pos);    // For large patterns
    for (int k=0 ; k<tree->nchildren ; k++) {
        node = tree->children[k];
        Point pt = node->pos.last;

        if (tree->pos.last != PASS_MOVE && cfg_map[pt]-1 < LEN_PRIOR_CFG) {
            node->pv += PRIOR_CFG[cfg_map[pt]-1];
            node->pw += PRIOR_CFG[cfg_map[pt]-1];
        }

        int height = line_height(pt);  // 0-indexed
        if (height <= 2 && empty_area(&tree->pos, pt, 3)) {
            // No stones around; negative prior for 1st + 2nd line, positive
            // for 3rd line; sanitizes opening and invasions
            if (height <= 1) {
                node->pv += PRIOR_EMPTYAREA;
                node->pw += 0;
            }
            if (height == 2) {
                node->pv += PRIOR_EMPTYAREA;
                node->pw += PRIOR_EMPTYAREA;
            }
        }

        fix_atari(&node->pos, pt, SINGLEPT_OK, TWOLIBS_TEST, !TWOLIBS_EDGE_ONLY,
                                                                 moves, sizes);
        if (slist_size(moves) > 0) {
            node->pv += PRIOR_SELFATARI;
            node->pw += 0;  // negative prior
        }

        double patternprob = large_pattern_probability(pt);
        if (patternprob > 0.0) {
            double pattern_prior = sqrt(patternprob);       // tone up
            node->pv += pattern_prior * PRIOR_LARGEPATTERN;
            node->pw += pattern_prior * PRIOR_LARGEPATTERN;
        }
    }

    if (tree->nchildren == 0) {
        // No possible move, add a pass move
        pos2 = tree->pos;
        pass_move(&pos2);
        tree->children[0] = new_tree_node(&pos2);
        tree->nchildren = 1;
    }
}

void free_tree(TreeNode *tree)
// Free memory allocated for the tree
{
    if (tree->children != NULL) {
        for (TreeNode **child = tree->children ; *child != NULL ; child++)
                free_tree(*child);
        free(tree->children);
    }
    free(tree);
}

double rave_urgency(TreeNode *node)
{
    double v = node->v + node->pv;
    double expectation = (node->w + node->pw)/v;
    if (node->av==0) return expectation;

    double rave_expectation = (double) node->aw / (double) node->av;
    double beta = node->av / (node->av + v + (double)v*node->av/RAVE_EQUIV);
    return beta * rave_expectation + (1-beta) * expectation;
}

double winrate(TreeNode *node)
{
    double wr;
    if (node->v>0) wr = (double) node->w / (double) node->v;
    else           wr = -0.1;
    return wr;
}

TreeNode* best_move(TreeNode *tree, TreeNode **except)
// best move is the most simulated one (avoiing nodes in except list
{
    int vmax=-1;
    TreeNode *best=NULL;

    if (tree->children == NULL) return NULL;

    for (TreeNode **child = tree->children ; *child != NULL ; child++) {
        if ((*child)->v > vmax) {
            int update = 1;
            if (except != NULL) 
                for (TreeNode **n=except ; *n!=NULL ; n++)
                    if (*child == *n) update=0;
            if (update) {
                vmax = (*child)->v;
                best = (*child);
            }
        }
    }
    return best;
}

TreeNode* most_urgent(TreeNode **children, int nchildren, int disp)
{
    int    k=0;
    double urgency, umax=0;
    TreeNode *urgent = children[0];

    // Randomize the order of the nodes
    SHUFFLE(TreeNode *, children, nchildren);

    for (TreeNode **child = children ; *child != NULL ; child++) {
        if (disp)
            dump_subtree(*child, N_SIMS/50, "", stderr, 0);
        urgency = rave_urgency(*child);
        if (urgency > umax) {
            umax = urgency;
            urgent = *child;
        }
        k++;
    }
    return urgent;
}

int tree_descend(TreeNode *tree, int amaf_map[], int disp, TreeNode **nodes)
// Descend through the tree to a leaf
{
    int last=0, passes = 0;
    Point move;
    //tree->v += 1;
    nodes[last] = tree;
   
    while (nodes[last]->children != NULL && passes <2) {
        if (disp) print_pos(&nodes[last]->pos, stderr, NULL);
        // Pick the most urgent child
        TreeNode *node = most_urgent(nodes[last]->children, 
                                            nodes[last]->nchildren, disp);
        nodes[++last] = node;
        move = node->pos.last;
        if (disp) { fprintf(stderr, "chosen "); ppoint(move); }

        if (move == PASS_MOVE) passes++;
        else {
            passes = 0;
            if (amaf_map[move] == 0) //Mark the point with 1 for black
                amaf_map[move] = (nodes[last-1]->pos.n%2==0 ? 1 : -1);
        }

        if (node->children == NULL && node->v >= EXPAND_VISITS)
            expand(node);
    }
    return last;
}

void tree_update(TreeNode **nodes,int last,int amaf_map[],double score,int disp)
// Store simulation result in the tree (nodes is the tree path)
{
    for (int k=last ; k>=0 ; k--) {     // walk nodes from leaf to the root
        TreeNode *n= nodes[k];
        if(disp) {
            char str[8]; str_coord(n->pos.last,str);
            fprintf(stderr, "updating %s %d\n", str, score<0.0); 
        }
        n->v += 1;         // TODO put it in tree_descend when parallelize
        n->w += score<0.0; // score is for to-play, node stats for just-played
        
        // Update the node children AMAF stats with moves we made 
        // with their color
        int amaf_map_value = (n->pos.n %2 == 0 ? 1 : -1);
        if (n->children != NULL) {
            for (TreeNode **child = n->children ; *child != NULL ; child++) {
                if ((*child)->pos.last == 0) continue;
                if (amaf_map[(*child)->pos.last] == amaf_map_value) {
                    if (disp) {
                        char str[8];
                        str_coord((*child)->pos.last, str);
                        fprintf(stderr, "  AMAF updating %s %d\n", str,score>0);
                    }
                    (*child)->aw += score > 0; // reversed perspective
                    (*child)->av += 1;
                }
            }
        }
        score = -score;
    }
}

Point tree_search(TreeNode *tree, int n, int owner_map[], int disp)
// Perform MCTS search from a given position for a given #iterations
{
    double s;
    int *amaf_map=calloc(BOARDSIZE, sizeof(int)), i, last; 
    TreeNode *best, *nodes[500];

    // Initialize the root node if necessary
    if (tree->children == NULL) expand(tree);
    memset(owner_map,0,BOARDSIZE*sizeof(int));

    for (i=0 ; i<n ; i++) {
        memset(amaf_map, 0, BOARDSIZE*sizeof(int));
        if (i>0 && i % REPORT_PERIOD == 0) print_tree_summary(tree, i, stderr); 
        last = tree_descend(tree, amaf_map, disp, nodes);
        Position pos = nodes[last]->pos;
        s = mcplayout(&pos, amaf_map, owner_map, disp);
        tree_update(nodes, last, amaf_map, s, disp);
        // Early stop test
        double best_wr = winrate(best_move(tree, NULL));
        if ( (i>n*0.05 && best_wr > FASTPLAY5_THRES)
              || (i>n*0.2 && best_wr > FASTPLAY20_THRES)) break;
    }
    dump_subtree(tree, N_SIMS/50, "", stderr, 1);
    print_tree_summary(tree, i, stderr);
    best = best_move(tree, NULL);

    free(amaf_map);
    if (best->pos.last == PASS_MOVE && best->pos.last2 == PASS_MOVE)
        return PASS_MOVE;
    else if (((double) best->w / (double) best->v) < RESIGN_THRES) 
        return RESIGN_MOVE;
    else
        return best->pos.last;
}

//============================= user interface(s) =============================

//----------------------------- utility routines ------------------------------
void make_pretty(Position *pos, char pretty_board[BOARDSIZE], int *capB
                                                                , int *capW)
{
    if ((pos->n)%2) { // WHITE to play
        for (int k=0 ; k<BOARDSIZE ; k++)
            if (pos->color[k] == 'X')      pretty_board[k] = 'O';
            else if (pos->color[k] == 'x') pretty_board[k] = 'X';
            else                           pretty_board[k] = pos->color[k];
        *capB = pos->cap;
        *capW = pos->capX;
    }
    else { // BLACK to play
        for (int k=0 ; k<BOARDSIZE ; k++)
            if (pos->color[k] == 'x') pretty_board[k] = 'O';
            else                      pretty_board[k] = pos->color[k];
        *capW = pos->cap;
        *capB = pos->capX;
    }
}

void dump_subtree(TreeNode *node, double thres, char *indent, FILE *f
                                                                , int recurse)
// print this node and all its children with v >= thres.
{
    char str[8], str_winrate[8], str_rave_winrate[8];
    str_coord(node->pos.last, str);
    if (node->v) sprintf(str_winrate, "%.3f", winrate(node)); 
    else         sprintf(str_winrate, "nan");
    if (node->av) sprintf(str_rave_winrate, "%.3f", (double)node->aw/node->av); 
    else          sprintf(str_rave_winrate, "nan");
    fprintf(f,"%s+- %s %5s "
              "(%6d/%-6d, prior %3d/%-3d, rave %6d/%-6d=%5s, urgency %.3f)\n"
           ,indent, str, str_winrate, node->w, node->v, node->pw, node->pv
           ,node->aw, node->av, str_rave_winrate, rave_urgency(node));
    if (recurse) {
        char new_indent[BUFLEN];
        sprintf(new_indent,"%s   ", indent);
        for (TreeNode **child = node->children ; *child != NULL ; child++)
            if ((*child)->v >= thres)
                dump_subtree((*child), thres, new_indent, f, 0);
    }
}

void print_tree_summary(TreeNode *tree, int sims, FILE *f)
{
    char best_seq[32]="", can[128]="", str[8], tmp[32];
    int k;
    TreeNode *best_node[5]={0,0,0,0,0}, *node;

    for (k=0 ; k<5 ; k++) { 
        best_node[k] = best_move(tree, best_node);
        if (best_node[k] != NULL) {
            str_coord(best_node[k]->pos.last,str);
            if (best_node[k]->v)
                sprintf(tmp, " %s(%.3f)", str, winrate(best_node[k]));
            else
                sprintf(tmp, " %s(nan)", str);
            strcat(can, tmp);
        }
    }
    node = tree;
    for (k=0 ; k<5 ; k++) {
        node = best_move(node, NULL);
        if (node == NULL) break;
        str_coord(node->pos.last, str);
        strcat(str, " ");
        strcat(best_seq, str);
    }
    fprintf(f,"[%4d] winrate %.3f | seq %s| can %s\n",sims
                                    ,winrate(best_node[0]), best_seq, can);   
}

Point parse_coord(char *s)
{
    char c;
    int  x,y;
    if(strcasecmp(s,"pass") == 0) return PASS_MOVE;
    sscanf(s, "%c%d", &c, &y);
    c = toupper(c);
    if (c<'J') x = c-'@';
    else       x = c-'@'-1;
    return (N-y+1)*(N+1) + x;
}

char* str_coord(Point pt, char str[8])
{
    if (pt == PASS_MOVE) strcpy(str, "pass");
    else if (pt == RESIGN_MOVE) strcpy(str, "resign");
    else {
        int row = pt/(N+1); int col = (pt%(N+1));
        sprintf(str, "%c%d", '@'+col,N+1-row);
        if (str[0] > 'H') str[0]++;
    }
    return str;
}

void ppoint(Point pt) {
    char str[8];
    str_coord(pt,str);
    fprintf(stderr,"%s\n",str);
}

void print_pos(Position *pos, FILE *f, int *owner_map)
// Print visualization of the given board position
{
    char pretty_board[BOARDSIZE], strko[8];
    int  capB, capW;

    make_pretty(pos, pretty_board, &capB, &capW);
    fprintf(f,"Move: %-3d   Black: %d caps   White: %d caps   Komi: %.1f",
            pos->n, capB, capW, pos->komi); 
    if (pos->ko)
        fprintf(f,"   ko: %s", str_coord(pos->ko,strko)); 
    fprintf(f,"\n");

    for (int row=1, k=N+1, k1=N+1 ; row<=N ; row++) {
        if (pos->last == k+1) fprintf(f, " %-2d(", N-row+1); 
        else                  fprintf(f, " %-2d ", N-row+1);
        k++;k1++;
        for (int col=1 ; col<=N ; col++,k++) {
            fprintf(f, "%c", pretty_board[k]);
            if (pos->last == k+1)    fprintf(f, "(");
            else if (pos->last == k) fprintf(f, ")");
            else                     fprintf(f, " ");
        }
        if (owner_map) {
            fprintf(f, "   ");
            for (int col=1 ; col<=N ; col++,k1++) {
                char c;
                if      ((double)owner_map[k1] > 0.6*N_SIMS) c = 'X';
                else if ((double)owner_map[k1] > 0.3*N_SIMS) c = 'x';
                else if ((double)owner_map[k1] <-0.6*N_SIMS) c = 'O';
                else if ((double)owner_map[k1] <-0.3*N_SIMS) c = 'o';
                else                                         c = '.';
                fprintf(f, " %c", c);
            }
        }
        fprintf(f, "\n");
    }
    fprintf(f, "    ");
    for (int col=1 ; col<=N ; col++) fprintf(f, "%c ", colstr[col]);
    fprintf(f, "\n\n");
}

//----------------------------- Various main programs -------------------------
double mcbenchmark(int n, Position *pos, int amaf_map[], int owner_map[])
// run n Monte-Carlo playouts from empty position, return avg. score
{
    double sumscore = 0.0;
    for (int i=0 ; i<n ; i++) {
        if (i%10 == 0) {
            if (i%50 == 0) fprintf(stderr, "\n%5d", i);
            fprintf(stderr, " ");
        }
        fprintf(stderr, ".");
        empty_position(pos); memset(amaf_map, 0, BOARDSIZE*sizeof(int)); 
        sumscore += mcplayout(pos, amaf_map, owner_map, 0); 
    }
    fprintf(stderr, "\n");
    return sumscore/n;
}

void begin_game(void) {
    c1++; c2=1;
    sprintf(buf,"BEGIN GAME %d, random seed = %u",c1,idum);
    log_fmt_s('I', buf, NULL);
}

void gtp_io(void)
{
    char line[BUFLEN], *cmdid, *command, msg[BUFLEN], *ret;
    char *known_commands="\ncputime\ndebug subcmd\ngenmove\nhelp\nknown_command"
    "\nlist_commands\nname\nplay\nprotocol_version\nquit\nversion\n";
    int      game_ongoing=1, i;
    int      *owner_map=calloc(BOARDSIZE, sizeof(int));
    TreeNode *tree;
    Position *pos, pos2;
    
    pos = &pos2;
    empty_position(pos);
    tree = new_tree_node(pos);

    for(;;) {
        ret = "";
        if (fgets(line, BUFLEN, stdin) == NULL) break;
        line[strlen(line)-1] = 0;
        log_fmt_s('C', line, NULL);
        command = strtok(line, " \t\n");
        if (command == NULL) continue;          // ignore newline
        if (command[0] == '#') continue;        // ignore comment line
        if (sscanf(command, "%d", &i) == 1) {
            cmdid = command;
            command = strtok(NULL, " \t\n");
        }
        else
            cmdid = "";

        if (strcmp(command, "play")==0) {
            c2++; game_ongoing = 1;
            ret = strtok(NULL, " \t\n");            // color is ignored
            char *str = strtok(NULL, " \t\n");
            if(str == NULL) goto finish_command;
            Point pt = parse_coord(str);
            if (pos->color[pt] == '.')
                ret = play_move(pos, pt);           // suppose alternate play
            else {
                if(pt == PASS_MOVE) ret = pass_move(pos);
                else ret ="Error Illegal move: point not EMPTY\n";
            }
        }
        else if (strcmp(command, "genmove") == 0) {
            c2++; game_ongoing = 1;
            Point pt;
            if (pos->last == PASS_MOVE && pos->n>2) {
                log_fmt_s('I', "Opponent pass. I pass", NULL);
                pt = PASS_MOVE;
            }
            else {
                free_tree(tree);
                tree = new_tree_node(pos);
                pt = tree_search(tree, N_SIMS, owner_map, 0);
            }
            if (pt == PASS_MOVE)
                pass_move(pos);
            else if (pt != RESIGN_MOVE)
                play_move(pos, pt);
            ret = str_coord(pt, buf);  
        }
        else if (strcmp(command, "cputime") == 0) {
            struct tms t;
            times(&t);
            float clock_ticks = sysconf(_SC_CLK_TCK);
            sprintf(buf, "%.3f", (t.tms_utime+t.tms_stime)/clock_ticks);
            ret = buf;
        }
        else if (strcmp(command, "clear_board") == 0) {
            if (game_ongoing) begin_game();
            game_ongoing = 0;
            free_tree(tree);
            ret = empty_position(pos);
            tree = new_tree_node(pos);
        }
        else if (strcmp(command, "boardsize") == 0) {
            char *str = strtok(NULL, " \t\n");
            if(str == NULL) goto finish_command;
            int size = atoi(str);
            if (size != N) {
                sprintf(buf, "Error: Trying to set incompatible boardsize %s"
                             " (!= %d)", str, N);
                log_fmt_s('E', buf, NULL);
                ret = buf;
            }
            else
                ret = "";
        }
        else if (strcmp(command,"debug") == 0)
            ret = debug(pos);
        else if (strcmp(command,"name") == 0)
            ret = "michi-c";
        else if (strcmp(command,"version") == 0)
            ret = "simple go program demo";
        else if (strcmp(command,"protocol_version") == 0)
            ret = "2";
        else if (strcmp(command,"list_commands") == 0)
            ret = known_commands;
        else if (strcmp(command,"help") == 0)
            ret = known_commands;
        else if (strcmp(command,"known_command") == 0) {
            char *command = strtok(NULL, " \t\n");
            if (strstr(known_commands,command) != NULL)
                ret = "true";
            else
                ret = "false";
        }
        else if (strcmp(command,"quit") == 0) {
            printf("=%s \n\n", cmdid);
            log_hashtable_synthesis();
            break;
        }
        else {
            sprintf(msg, "Warning: Ignoring unknown command - %s\n", command);
            ret = msg;
        }
        print_pos(pos, stderr, owner_map);
finish_command:
        if ((ret[0]=='E' && ret[1]=='r')
                        || ret[0]=='W') printf("\n?%s %s\n\n", cmdid, ret);
        else                            printf("\n=%s %s\n\n", cmdid, ret);
        fflush(stdout);
    }
}

int main (int argc, char *argv[]) 
{
    char *command;
    // Init global data
    flog = fopen("michi.log", "w");
    setbuf(flog, NULL);                // guarantees that log is unbuffered
    make_pat3set();
    init_large_patterns();
    already_suggested = calloc(1, sizeof(Mark));
    mark1 = calloc(1, sizeof(Mark)); mark2 = calloc(1, sizeof(Mark));
    Position *pos = malloc(sizeof(Position));
    int      *amaf_map=calloc(BOARDSIZE, sizeof(int)); 
    int      *owner_map=calloc(BOARDSIZE, sizeof(int));
    empty_position(pos);
    TreeNode *tree = new_tree_node(pos);
    expand(tree);
    slist_clear(allpoints);
    FORALL_POINTS(pos,pt)
        if (pos->color[pt] == '.') slist_push(allpoints,pt);

    // check if the user gave a seed for the random generator
    if (argc == 3) {
        sscanf(argv[1], "-z%u", &idum);
        if (idum == 0)
            idum = true_random_seed();
        command = argv[2];
    }
    else
        command = argv[1];

    if (argc < 2)    // default action
        usage();
    else if (strcmp(command,"gtp") == 0)
        gtp_io();
    else if (strcmp(command,"mcdebug") == 0)
        printf("%lf\n", mcplayout(pos, amaf_map, owner_map, 1)); 
    else if (strcmp(command,"mcbenchmark") == 0)
        printf("%lf\n", mcbenchmark(2000, pos, amaf_map, owner_map)); 
    else if (strcmp(command,"tsdebug") == 0) {
        Point move=tree_search(tree, 100, amaf_map, 0);
        fprintf(stderr, "move = %s\n", str_coord(move,buf));
        if (move != PASS_MOVE && move != RESIGN_MOVE)
            play_move(&tree->pos,move);
        print_pos(&tree->pos, stderr, NULL); 
    }
    else
        usage();
    free_tree(tree); free(pos);
    free(amaf_map); free(owner_map);
    free(already_suggested); free(mark1); free(mark2);
    fclose(flog);
    return 0;
}
