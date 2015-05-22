//
//  michi-ios.h
//  Mingo
//
//  Created by Horace Ho on 2015/05/15.
//  Copyright (c) 2015 Horace Ho. All rights reserved.
//

#import <UIKit/UIKit.h>

typedef enum GoStone : NSUInteger {
    kEmpty,
    kBlack,
    kWhite
} GoStone;

struct GoPoint {
    NSInteger col;
    NSInteger row;
};
typedef struct GoPoint GoPoint;

@interface Michi : NSObject

+ (instancetype)one;

@property (strong, nonatomic) NSString *version;

- (NSInteger)size;
- (void)setup;
- (void)reset;
- (BOOL)playMove:(GoPoint)point;
- (void)autoMove;
- (BOOL)isUndoOK;
- (void)undoMove;

- (GoStone)whosTurn;
- (GoStone)stoneAt:(GoPoint)point;

- (NSString *)stringFromPoint:(CGPoint)point;
- (NSString *)stringFromCol:(NSInteger)col;
- (NSString *)stringFromRow:(NSInteger)row;

@end
