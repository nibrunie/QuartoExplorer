# QuartoExplorer
A (supposed to be) fast explorer of Quarto Game, focusing on some low-level optimizations

#Description
The goal of this program is to solved Quarto. 
That is to determine from a starting position (or any position for that matter) which will be the winning player.

This programs is designed as an utility to explore quarto play possibilities.
To learn more about Quarto board game: https://en.wikipedia.org/wiki/Quarto_(board_game)

By applying symetries, the number of starting position as been reduce to 1 possible piece (called piece 0) and 3 possible positions (0, 1 and 5)
The <player 0> starts and give the piece <0> to <player 1>.
After that is the infinite universe, <player 0> has to decide where to set piece <0>, and then chose and a new piece to give to <player 1> so he can set it on the board an so on and so force.
Until the board is full or a 4-piece pattern "Quarto" appears.



This program has been developped to play with avx-2 and try to be as fast as possible.

#Requirements:
-> Your processor must support avx2 instruction set (at least Intel's Haswell generation or equivalent)

#How To:
-> build using "make"
-> run using "./quarto_ia"
-> an interactive menu will be displayed in the shell
   -> e.g chose: "10" to launch exploration from the first position


#Internals
In QuartoExplorer, The state of a quarto board is encoded in several ways.
The easiest way to understand is the expanded version.
In this version the board is encoded by two 64-bit values.
The first one, generally called pos_used_maskx4 is the 4-time replication of a 16-bit masks which indicates whose of the 16 positions on the board is occupied.
The second one, generally called pawn_state_mask is made of four 16-bit masks.
Each bit in a mask indicates a state of a piece in the given position.
The first mask (16 LSB) indicates the height of the pieces on the 16 board positions (high: 1, low: 1).
The second indicates if the pieces have a hole (no hole: 1, hole: 0).
The third one indicates the piece color (dark: 1, light: 0). 
The last one (16 MSB) indicates if the pieces are round or square (round: 0, square: 1).
For example the bit 0 of MSB masks at 1 means the piece at position 0 is round.
Obviously a bit in the pawn_state_mask masks is only meaningful if the corresponding bit of the pos_used_maskx4 is set, else it means there is no piece in this position.

## Exploration phase
The exploration phase is based on a hash table 
A state of the quarto board is encoded as 82-bit divided as follows:
---------------------------------------------
| 18-bit hash | 62-bit value | 2-bit status |
---------------------------------------------
the 78-bit hash+value encode the state: which pieces are on the board and where.
the 2-bit status encode which is the future of a perfect game starting from the state: Win of player 0 or 1 or tie.
The hash table store 64-bit : value+status.
The hash table is organized as an array of list.
The array is indexed by the hash, each cell contains a list (value+status).

The state of a quarto board can be encoded on fewer than 82-bit.
You need 2-bit for the position and piece <0> and 4-bit for each of the 15 remaining piece. 
For a full board, less than 62 bits are required>
But is is impratical to store and compare non power of two bytes long values.
So I chose 64-bit at the hash table value container.
This makes it possible to use vector/simd instructions to speed-up the look-up in the hashtable.

## Quarto Checking

# Program Commands

 1   : display current position 
 2   : get next pawn to play   
 3   : get next position to play
 4   : play position           
 5   : set player id          
 6   : play optimal game     
 7   : request hash for information on current position 
 8   : request hash for information on arg position    
 9   : clear position and pawn masks                  
 10  : explore plays from position 0                 
 11  : explore plays from position 1                
 12  : explore plays from position 2               
 13  : play random                                
 14  : check position for quarto                 
 17  : stop     
