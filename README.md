# QuartoExplorer
A (supposed to be) fast explorer of Quarto Game, focusing on some low-level optimizations
The goal of this program is to solved Quarto. 
That is to determine from a starting position (or any position for that matter) which will be the winning player.

This programs is designed as an utility to explore quarto play possibilities.
To learn more about Quarto board game: https://en.wikipedia.org/wiki/Quarto_(board_game)

By applying symetries, the number of starting position as been reduce to 1 possible piece (called piece 0) and 3 possible positions (0, 1 and 5)
The <player 0> starts and give the piece <0> to <player 1>.
After that is the infinite universe, <player 0> has to decide where to set piece <0>, and then chose and a new piece to give to <player 1> so he can set it on the board an so on and so force.
Until the board is full or a 4-piece pattern "Quarto" appears.



This program has been developped to play with avx-2 and try to be as fast as possible.
