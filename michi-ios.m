#import "michi.h"
#import "michi-ios.h"

static TreeNode *tree;
static Position *pos, pos2;
static int *owner_map;
static bool can_undo;

char* empty_position(Position *pos);
TreeNode* new_tree_node(Position *pos);
void free_tree(TreeNode *tree);
Point tree_search(TreeNode *tree, int n, int owner_map[], int disp);

void undo_move(Position *pos);

@interface Michi()

@end

@implementation Michi

+ (instancetype)one
{
    static dispatch_once_t once;
    static Michi *sharedInstance;
    dispatch_once(&once, ^{
        sharedInstance = [[Michi alloc] init];
        flog = stdout;
        mark1 = calloc(1, sizeof(Mark));
        mark2 = calloc(1, sizeof(Mark));
        make_pat3set();
        NSString *probPath = [[NSBundle mainBundle] pathForResource:@"patterns" ofType: @"prob"];
        NSString *spatPath = [[NSBundle mainBundle] pathForResource:@"patterns" ofType: @"spat"];
        init_large_patterns(probPath.UTF8String, spatPath.UTF8String);
        already_suggested = calloc(1, sizeof(Mark));
        pos = &pos2;
        empty_position(pos);
        tree = new_tree_node(pos);
        owner_map = calloc(BOARDSIZE, sizeof(int));
        can_undo = false;
    });
    return sharedInstance;
}

- (id)init {
    self = [super init];
    if (self) {
        self.version = @"1.0";
    }
    return self;
}

- (NSInteger)size {
    return N;
}

- (BOOL)playMove:(GoPoint)point {
    NSLog(@"%s col: %ld row: %ld", __func__, (long)point.col, (long)point.row);
    BOOL ok = NO;
    NSInteger offset = (point.row+1) * (self.size+1) + (point.col+1);
    if (pos->color[offset] == '.') {
        int moves = pos->n;
        play_move(pos, (Point)offset);
        if (pos->n > moves) can_undo = true;
    }
    return ok;
}

- (GoStone)whosTurn {
    return ((pos->n)%2) ? kWhite : kBlack;
}

- (GoStone)stoneAt:(GoPoint)point {
    GoStone stone = kEmpty;
    NSInteger offset = (point.row+1) * (self.size+1) + (point.col+1);
    if (self.whosTurn == kWhite) {
        if (pos->color[offset] == 'X') {
            stone = kWhite;
        } else if (pos->color[offset] == 'x') {
            stone = kBlack;
        }
    } else { // BLACK to play
        if (pos->color[offset] == 'X') {
            stone = kBlack;
        } else if (pos->color[offset] == 'x') {
            stone = kWhite;
        }
    }
    return stone;
}

- (void)autoMove {
    Point point = [self hint];
    [self play:point];
}

- (BOOL)isUndoOK {
    return can_undo;
}

- (void)undoMove {
    if (can_undo) {
        can_undo = false;
        undo_move(pos);
    }
}

- (Point)hint {
    Point point;
    if (pos->last == PASS_MOVE && pos->n>2) {
         point = PASS_MOVE;
    }
    else {
        free_tree(tree);
        tree = new_tree_node(pos);
        point = tree_search(tree, N_SIMS, owner_map, 0);
    }

    char buffer[5];
    NSLog(@"%s %s", __func__, str_coord(point, buffer));

    return point;
}

- (void)play:(Point)point {
    int moves = pos->n;
    if (point == PASS_MOVE) {
        pass_move(pos);
    } else {
        play_move(pos, point);
    }
    if (pos->n > moves) can_undo = true;
}

- (void)reset {
    free_tree(tree);
    empty_position(pos);
    tree = new_tree_node(pos);
    can_undo = false;
}

- (NSString *)stringFromCol:(NSInteger)col {
    NSString *cols = @"ABCDEFGHJKLMNOPQRSTUV";
    return [cols substringWithRange:NSMakeRange(col, 1)];
}

- (NSString *)stringFromRow:(NSInteger)row {
    return [NSString stringWithFormat:@"%ld", (long)(self.size - row)];
}

- (NSString *)stringFromPoint:(CGPoint)point {
    NSString *string = [NSString stringWithFormat:@"%@%@", [self stringFromCol:point.x], [self stringFromRow:point.y]];
    return string;
}

@end
