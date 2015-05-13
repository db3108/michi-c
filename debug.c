#include "michi.h"
extern char buf[BUFLEN];

//============================= messages logging ==============================
FILE    *flog;             // FILE to log messages
int     c1,c2;             // counters for warning messages
int     nmsg;              // number of written log entries

void too_many_msg(void)
// if too many messages have been logged, print a last error and exit 
{
    fprintf(stderr,"Too many messages have been written in log file "
                       " (maximum 100000)\n");
    fprintf(flog,"Too many messages (maximum 100000)\n");
    exit(-1);
}

void log_fmt_s(char type, const char *msg, const char *s)
// Log a formatted message (string parameter)
{
    fprintf(flog, "%c %5d/%3.3d ", type, c1, c2);
    fprintf(flog, msg, s); fprintf(flog, "\n");
    if(type == 'E') {
        fprintf(stderr,"%c %5d/%3.3d ", type, c1, c2);
        fprintf(stderr, msg, s); fprintf(stderr,"\n");
    }
    if(nmsg++ > 1000000) too_many_msg();
}

void log_fmt_i(char type, const char *msg, int n)
// Log a formatted message (int parameter)
{
    sprintf(buf, msg, n);
    log_fmt_s(type, "%s", buf);
}

void log_fmt_p(char type, const char *msg, Point pt)
// Log a formatted message (point parameter)
{
    char str[8];
    sprintf(buf, msg, str_coord(pt,str));
    log_fmt_s(type,"%s", buf);
}

//============================= debug subcommands =============================

char decode_env4(int env4, int pt)
{
    int hi, lo;
    env4 >>= pt;
    hi = (env4>>4) & 1;
    lo = env4 & 1;
    return (hi<<1) + lo;
}

char decode_env8(int env8, int pt)
{
    int c;
    if (pt >= 4)
        c = decode_env4(env8>>8, pt-4);
    else
        c = decode_env4(env8&255, pt);
    switch(c) {
        case 0: return 'O';
        case 1: return 'X';
        case 2: return '.';
        case 3: return '#';
    }
    return 0;
}

void print_env8(int env8)
{
    char src[10];                           // src 0 1 2   bits in env8  7 0 4
    src[0] = decode_env8(env8, 7); // NW           3 4 5   bit 0=LSB     3 . 1
    src[1] = decode_env8(env8, 0); // N            6 7 8                 6 2 5
    src[2] = decode_env8(env8, 4); // NE
    src[3] = decode_env8(env8, 3); // W 
    src[4] = '.';                  // Center
    src[5] = decode_env8(env8, 1); // E     
    src[6] = decode_env8(env8, 6); // SW
    src[7] = decode_env8(env8, 2); // S 
    src[8] = decode_env8(env8, 5); // SE

    printf("env8 = %d\n", env8);
    printf("%c %c %c\n", src[0], src[1], src[2]);
    printf("%c %c %c\n", src[3], src[4], src[5]);
    printf("%c %c %c\n", src[6], src[7], src[8]);
}

void print_marker(Position *pos, Mark *marker)
{
    Position pos2 =*pos;
    FORALL_POINTS(pos2, p)
        if (is_marked(marker, p)) pos2.color[p]='*';
    print_pos(&pos2, stdout, 0);
}

char* debug(Position *pos) 
{
    char *command = strtok(NULL," \t\n"), *ret="";
    char *known_commands = "\nenv8\nfix_atari\ngen_playout\nmatch_pat3\n"
                           "match_pat\nplayout\nprint_mark\nsavepos\nsetpos\n";
    int  amaf_map[BOARDSIZE], owner_map[BOARDSIZE];
    Info sizes[BOARDSIZE];
    Point moves[BOARDSIZE];

    if (strcmp(command, "setpos") == 0) {
        char *str = strtok(NULL, " \t\n");
        while (str != NULL) {
            Point pt = parse_coord(str);
            if (pos->color[pt] == '.')
                ret = play_move(pos, pt);        // suppose alternate play
            else if (strcmp(str,"pass")==0 || strcmp(str,"PASS")==0)
                ret = pass_move(pos);
            else
                ret ="Error Illegal move: point not EMPTY\n";
            str = strtok(NULL, " \t\n");
        }
    }
    else if (strcmp(command, "savepos") == 0) {
        char *filename = strtok(NULL, " \t\n");
        FILE *f=fopen(filename, "w");
        print_pos(pos, f, NULL);
        fclose(f);
        ret = "";
    }
    else if (strcmp(command, "playout") == 0)
        mcplayout(pos, amaf_map, owner_map, 1);
    else if (strcmp(command, "gen_playout") == 0) {
        char *suggestion = strtok(NULL, " \t\n");
        if (suggestion != NULL) {
            Point last_moves_neighbors[20];
            make_list_last_moves_neighbors(pos, last_moves_neighbors);
            if (strcmp(suggestion, "capture") == 0)
                gen_playout_moves_capture(pos, last_moves_neighbors,
                                            1.0, 0, moves, sizes);
            else if (strcmp(suggestion, "pat3") == 0)
                gen_playout_moves_pat3(pos, last_moves_neighbors,
                                            1.0, moves);
            ret = slist_str_as_point(moves); 
        }
        else
            ret = "Error - missing [capture|pat3]";
    }
    else if (strcmp(command, "match_pat") == 0) {
        char *str = strtok(NULL, " \t\n");
        if(str == NULL) ret = "Error missing point";
        else {
            copy_to_large_board(pos);
            Point pt = parse_coord(str);
            str = strtok(NULL, " \t\n");
            int verbose=1;
            if (str == NULL)
                verbose=0;
            ret = make_list_pat_matching(pt, verbose);
        }
    }
    else if (strcmp(command, "fix_atari") == 0) {
        int is_atari;
        char *str = strtok(NULL, " \t\n");
        if (str == NULL)
            return ret = "Error -- point missing";
        Point pt = parse_coord(str);
        if (pos->color[pt]!='x' && pos->color[pt]!='X') {
            ret ="Error given point not occupied by a stone";
            return ret;
        }
        is_atari = fix_atari(pos,pt, SINGLEPT_NOK, TWOLIBS_TEST,0,moves, sizes);
        slist_str_as_point(moves);
        int l = strlen(buf);
        for (int k=l+1 ; k>=0 ; k--) buf[k+1] = buf[k];
        if (l>0)
            buf[1] = ' ';
        if (is_atari) buf[0] = '1';
        else          buf[0] = '0';
        ret = buf;
    }
    else if (strcmp(command, "env8") == 0) {
        char *str = strtok(NULL, " \t\n");
        if(str == NULL) ret = "Error missing point";
        else {
            Point pt = parse_coord(str);
            int env8=(pos->env4d[pt]<<8) + pos->env4[pt];
            print_env8(env8);
        }
    }
    else if (strcmp(command, "print_mark") == 0) {
        char *str = strtok(NULL, " \t\n");
        Mark *marker=already_suggested;
        if (strcmp(str, "mark1") == 0) marker = mark1;
        if (strcmp(str, "mark2") == 0) marker = mark2;
        print_marker(pos, marker);
        ret = "";
    }
    else if (strcmp(command, "help") == 0)
        ret = known_commands;
    return ret;
}
