Michi-c --- Michi recoded in C
==============================

This is a recoding in C (for speed) of the michi.py code by Petr Baudis avalaible at 
https://github.com/pasky/michi.

Michi stands for "Minimalistic Pachi". This is a Minimalistic Go MCTS Engine. The aims of the project are best explained by the author: 

> Michi aims to be a minimalistic but full-fledged Computer Go program based
> on state-of-art methods (Monte Carlo Tree Search) and written in Python.
> Our goal is to make it easier for new people to enter the domain of
> Computer Go, peek under the hood of a "real" playing engine and be able
> to learn by hassle-free experiments - with the algorithms, add heuristics,
> etc.

> This is not meant to be a competitive engine; simplicity and clear code is
> preferred over optimization (after all, it's in Python!).  But compared to
> other minimalistic engines, this one should be able to beat beginner
> intermediate human players, and I believe that a *fast* implementation
> of exactly the same heuristics would be around 4k KGS or even better.

Please go on his project page to read more about Michi and to find some information about theory or interesting projects to do.

Michi-c is distributed under the MIT licence.  Now go forth, hack and peruse!
However, as stated above, the main goal of michi and michi-c is to provide a simple and clear code. Therefore, one objective is not to make michi-c grow bigger.
I would like to just correct bugs and/or modify the code to make it clearer or simpler.

Therefore, a companion project has been setup at 

https://github.com/db3108/michi-c2

in order to develop a program with better performances and more functionalities while relaxing the objective for brevity.

Installing
----------

When in the directory that contains michi.c and Makefile, just type the command
(whithout the $ sign)

$ make

This will build the michi executable.

If you have gogui (http://gogui.sourceforge.net/) installed on your system, define the GOGUI variable (export GOGUI=/path/to/gogui/bin) with the location where the gogui executables can be found. Then 

$ make test

will perform a few (quick) regression tests. The result should be :

    tests/run
    10 passed
    20 passed
    30 passed
    110 passed
    210 passed
    220 passed
    230 passed
    240 passed
    250 passed
    260 passed
    Summary: 10/10 passes. 0 unexpected passes, 0 unexpected failures
    10 passed
    20 passed
    30 passed
    40 passed
    50 passed
    60 passed
    70 passed
    Summary: 7/7 passes. 0 unexpected passes, 0 unexpected failures
 
Usage
-----

$ ./michi gtp

will allow to play a game using the gtp protocol. Type help to get the list of
available commands. 

However it's easier to use michi through the gogui graphical interface.

    http://gogui.sourceforge.net/

With gogui, you can also let michi play GNUGo:

    gogui/bin/gogui-twogtp -black './michi.py gtp' -white 'gnugo --mode=gtp --chinese-rules --capture-all-dead' -size 9 -komi 7.5 -verbose -auto

It is *highly* recommended that you download Michi large-scale pattern files
(patterns.prob, patterns.spat):

    http://pachi.or.cz/michi-pat/

Store and unpack them in the current directory for Michi to find.

You can also try

$ ./michi mcbenchmark

this will run 2000 random playouts

$ ./michi tsdebug

this will run 1 MCTS tree search.

All the parameters are hard coded in the michi.h file, which must be modified if you want to play with the code.

Understanding and Hacking
-------------------------

The C code can be read in parallel with the python code. 
I have been careful to keep the notations used by Petr (almost) everywhere.
Of course the algorithms are the same (at least functionally) as well as the
parameters. Most of the comments have been retained verbatim.

Examples where the python and the C codes are different are:
- in the functions gen_playout_moves_xxx(). I have not been able to emulate in 
  C the generators that are available in python (yield instruction). So these
  functions in the C code must compute the whole list of suggestions before 
  returning.
- computation of blocks does not use regexp as the direct coding is simple.
- need to recode a functionality equivalent to python dictionary (in patterns.c)

The source is composed in 6 independent parts in michi.c
- Utilities
- Board routines
- Go heuristics
- Monte Carlo Playout policy
- Monte Carlo Tree search
- User Interface (Utilities, Various main programs)

and pattern code (3x3 and large patterns) which is found in patterns.c

Go programs heavily use lists or sets of (small) integers. There are many possible implementations that differ by the performance of the various operations we need to perform on these data structures:
- insert one element,
- remove one element, 
- enumerate all the elements, 
- test "element belongs to ?", 
- make the set empty ...

The two simple implementations Slist and Mark of sets that I needed to make the michi-c program are found in michi.h (inlined for performance).

Note: Some other concise (but hopefully useful) explanations are given as more detailed comments in the codes themselves.

Short bibliography
------------------

1.  Martin Mueller, Computer Go, Artificial Intelligence, Vol.134, No 1-2,
    pp 145-179, 2002
2.  Remi Coulom.  Efficient Selectivity and Backup Operators in Monte-Carlo Tree
    Search.  Paolo Ciancarini and H. Jaap van den Herik.  5th International 
    Conference on Computer and Games, May 2006, Turin, Italy.  2006. 
    <inria-00116992>
3.  Sylvain Gelly, Yizao Wang, Remi Munos, Olivier Teytaud.  Modification of UCT
    with Patterns in Monte-Carlo Go. [Research Report] RR-6062, 2006.
    <inria-00117266v3>
4.  David Stern, Ralf Herbrich, Thore Graepel, Bayesian Pattern Ranking for Move
    Prediction in the Game of Go, In Proceedings of the 23rd international 
    conference on Machine learning, pages 873–880, Pittsburgh, Pennsylvania, 
    USA, 2006
5.  Rémi Coulom. Computing Elo Ratings of Move Patterns in the Game of Go. 
    In ICGA Journal (2007), pp 198-208.
6.  Sylvain Gelly, David Silver. Achieving Master Level Play in 9×9 Computer Go.
    Proceedings of the Twenty-Third AAAI Conference on Artificial Intelligence 
    (2008)
7.  Albert L Zobrist. A New Hashing Method with Application for Game Playing.
8.  Petr Baudis. MCTS with Information Sharing, PhD Thesis, 2011
9.  Robert Sedgewick, Algorithms in C, Addison-Wesley, 1990

and many other PhD thesis accessible on the WEB

Note: [ref 1] can be consulted for the definition of Computer Go terms : 
      points, blocks, eyes, false eyes, liberties, etc.
      and historical bibliography


