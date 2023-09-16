#include <ncurses/ncurses.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ROWS 16
#define COLS 8

//board constants
#define BITS_IN_ROW 8
const uint64_t first_col_mask = 0b0000000100000001000000010000000100000001000000010000000100000001;
const uint64_t last_col_mask = first_col_mask << (BITS_IN_ROW - 1);
const uint64_t first_row_mask = UINT8_MAX;
const uint64_t last_row_mask = (uint64_t)UINT8_MAX << (64 - 8);
#if (ROWS % 8 || COLS != 8)
    #error Number of rows must be divisible by 8
#endif
#define BITARRAY_SIZE ROWS/8
#define left true
#define right false

//shape constants
#define DEFAULT_POS 2 //default position of new object (reference character of shape at position [2,2])
const uint32_t shape_first_row_mask = 0b00000000000000000000000011111111;
const uint32_t shape_second_row_mask = 0b00000000000000001111111100000000;
const uint32_t shape_third_row_mask = 0b00000000111111110000000000000000;
const uint32_t shape_fourth_row_mask = 0b11111111000000000000000000000000;





typedef struct {
    union {
        uint64_t board_bitarray[2];
        uint8_t board_rows[16];
    };
} board;

typedef enum {S, Z, T, J, L, row, square} shape_type;

typedef union {
        uint32_t num_repr;
        uint8_t arr_repr[4];
} shape_form;

typedef struct {
    union {
        uint32_t num_repr;
        uint8_t arr_repr[4];
    };
    shape_type type;
    uint8_t rotation;
    uint8_t x_pos;
    uint8_t y_pos;
} shape;

board permanent_board = {0};
board temporary_board = {0};
shape current_shape = {0};
int score = 0;
uint32_t time_diff = 500000;


//All shapes in all different rotation modes
const shape_form* shapes[7] = {
    (shape_form[]){0b00000000000000000000001100000110, 0b00000000000001000000011000000010, 0b00000000000000110000011000000000, 0b00000000000000100000001100000001}, //S shape
    /*              O**OOOOO                                O*OOOOOO                                OOOOOOOO                                *OOOOOOO
                    **OOOOOO                                O**OOOOO                                O**OOOOO                                **OOOOOO                
                    OOOOOOOO                                OO*OOOOO                                **OOOOOO                                O*OOOOOO                
                    OOOOOOOO                                OOOOOOOO                                OOOOOOOO                                OOOOOOOO                            
    */              
    (shape_form[]){0b00000000000000000000011000000011, 0b00000000000000100000011000000100, 0b00000000000001100000001100000000, 0b00000000000000010000001100000010}, //Z shape
    /*              **OOOOOO                                OO*OOOOO                                OOOOOOOO                                O*OOOOOO                
                    O**OOOOO                                O**OOOOO                                **OOOOOO                                **OOOOOO                
                    OOOOOOOO                                O*OOOOOO                                O**OOOOO                                *OOOOOOO                
                    OOOOOOOO                                OOOOOOOO                                OOOOOOOO                                OOOOOOOO                            
    */
    (shape_form[]){0b00000000000000000000001000000111, 0b00000000000001000000011000000100, 0b00000000000001110000001000000000, 0b00000000000000010000001100000001}, //T shape
    /*              ***OOOOO                                OO*OOOOO                                OOOOOOOO                                *OOOOOOO                
                    O*OOOOOO                                O**OOOOO                                O*OOOOOO                                ***OOOOO                
                    OOOOOOOO                                OO*OOOOO                                ***OOOOO                                *OOOOOOO                
                    OOOOOOOO                                OOOOOOOO                                OOOOOOOO                                OOOOOOOO                            
    */
    (shape_form[]){0b00000000000000000000011100000001, 0b00000000000000100000001000000110, 0b00000000000001000000011100000000, 0b00000000000000110000001000000010}, //J shape
    /*              *OOOOOOO                                O**OOOOO                                OOOOOOOO                                O*OOOOOO                
                    ***OOOOO                                O*OOOOOO                                ***OOOOO                                O*OOOOOO                
                    OOOOOOOO                                O*OOOOOO                                OO*OOOOO                                **OOOOOO                
                    OOOOOOOO                                OOOOOOOO                                OOOOOOOO                                OOOOOOOO                
    */
    (shape_form[]){0b00000000000000000000011100000100, 0b00000000000001100000001000000010, 0b00000000000000010000011100000000, 0b00000000000000100000001000000011}, //L shape
    /*              OO*OOOOO                                O*OOOOOO                                OOOOOOOO                                **OOOOOO                
                    ***OOOOO                                O*OOOOOO                                ***OOOOO                                O*OOOOOO                
                    OOOOOOOO                                O**OOOOO                                *OOOOOOO                                O*OOOOOO                
                    OOOOOOOO                                OOOOOOOO                                OOOOOOOO                                OOOOOOOO                
    */
    (shape_form[]){0b00000000000000000000111100000000, 0b00000100000001000000010000000100, 0b00000000000011110000000000000000, 0b00000010000000100000001000000010}, //row shape
    /*              OOOOOOOO                                OO*OOOOO                                OOOOOOOO                                O*OOOOOO                
                    ****OOOO                                OO*OOOOO                                OOOOOOOO                                O*OOOOOO                
                    OOOOOOOO                                OO*OOOOO                                ****OOOO                                O*OOOOOO                
                    OOOOOOOO                                OO*OOOOO                                OOOOOOOO                                O*OOOOOO                
    */ 
    (shape_form[]){0b00000000000000000000001100000011} //square shape
    /*              **OOOOOO                
                    **OOOOOO                
                    OOOOOOOO                
                    OOOOOOOO                
    */
};

/**
 * @brief Function performs shift through all bitarrays
 * @param direction Direction (left or right)
 * @param current_board Board to shift
*/
inline static void crossarray_line_shift(bool direction, board* current_board){
    if (direction == left){
        int current_bitarray_index = 0;
        uint8_t upper_last_row = 0;
        uint8_t lower_last_row = 0;

        //current bitarray is the lower one from pair
        for (int lower_last_row_index = BITS_IN_ROW - 1; lower_last_row_index < ROWS; lower_last_row_index += BITS_IN_ROW){
            lower_last_row = current_board->board_rows[lower_last_row_index];
            current_board->board_bitarray[current_bitarray_index] <<= BITS_IN_ROW;
            current_board->board_rows[lower_last_row_index - 7] = upper_last_row;
            upper_last_row = lower_last_row;
            ++current_bitarray_index;
        }
    }

    else {
        int current_bitarray_index = BITARRAY_SIZE - 1;
        uint8_t upper_first_row = 0;
        uint8_t lower_first_row = 0;

        //current bitarray is the upper one from pair
        for (int upper_first_row_index = ROWS - BITS_IN_ROW; upper_first_row_index >= 0; upper_first_row_index -= BITS_IN_ROW){
            upper_first_row = current_board->board_rows[upper_first_row_index];
            current_board->board_bitarray[current_bitarray_index] >>= BITS_IN_ROW;
            current_board->board_rows[upper_first_row_index + 7] = lower_first_row;
            lower_first_row = upper_first_row;
            --current_bitarray_index;
        }
    }
}

/**
 * @brief Function shifts bitarray with specified index. If an overflow occurs, it writes 
 * overflowed bits to the next (or previous) bitarray. 
 * NEXT (OR PREVIOUS) ARRAY'S FIRST ROW WILL BE OVERWRITTEN!!!
 * @param direction Direction of shift
 * @param current_board Board to shift
 * @param index Index of bitarrray which will be shifted
*/
void overflow_override_shift(bool direction, board* current_board, int index){
    if (direction == left){
        if (current_board->board_bitarray[index] & last_row_mask && index < BITARRAY_SIZE){
            int last_row_index = ((index + 1) * BITS_IN_ROW) - 1;
            current_board->board_rows[last_row_index + 1] = current_board->board_rows[last_row_index];
        }
        current_board->board_bitarray[index] <<= BITS_IN_ROW;
    }

    else if (direction == right){
        if (current_board->board_bitarray[index] & first_col_mask && index != 0){
            int first_row_index = index * BITS_IN_ROW;
            current_board->board_rows[first_row_index - 1] = current_board->board_rows[first_row_index];
        }
        current_board->board_bitarray[index] >>= BITS_IN_ROW;
    }
}

/**
 * @brief Function moves down all rows above the full row 
 * @param full_row Index of full row
*/
void MoveRows(int full_row){
    int board_index = full_row / COLS;
    uint64_t mask = ((uint64_t)1 << (BITS_IN_ROW * (full_row % BITS_IN_ROW))) - 1;
    uint64_t new_bitboard = (permanent_board.board_bitarray[board_index] & mask) << BITS_IN_ROW;
    
    permanent_board.board_bitarray[board_index] &= ~mask;
    permanent_board.board_bitarray[board_index] |= new_bitboard;

    for (int i = board_index - 1; i >= 0; --i){
        overflow_override_shift(left, &permanent_board, i);
    }

}

/**
 * @brief Function recursively search and removes full rows. Wrapping function for MoveRows
 * @return Number of removed rows
*/
int RemoveFullRows(){
    int moved_rows = 0;
    for (int x = current_shape.x_pos - 2; x < ROWS; ++x){
        if (permanent_board.board_rows[x] == UINT8_MAX){
            permanent_board.board_rows[x] = 0;
            MoveRows(x);
            ++moved_rows;
        }
    }
    if (moved_rows){
        moved_rows += RemoveFullRows();
    }
    return moved_rows;
}

/**
 * @brief Moves shape one row downwards. If the shape has landed, writes it to permanemt_board and
 * checks for full rows
 * @return Whether the shape has landed
*/
bool Descend(){
    int new_score = 0;
    bool landed = false;
    if (temporary_board.board_rows[ROWS - 1]){ //if shape comes to the bottom of gameboard
        permanent_board.board_bitarray[BITARRAY_SIZE - 1] |= temporary_board.board_bitarray[BITARRAY_SIZE - 1];
        landed = true;

        if (permanent_board.board_rows[ROWS - 1] == UINT8_MAX){ //if the last row is full
            new_score += RemoveFullRows();
        }
    }
    
    else {
        board test_board;
        memcpy(&test_board, &temporary_board, sizeof(board));
        crossarray_line_shift(left, &test_board); //shape is moved one row down

        for (int i = 0; i < BITARRAY_SIZE; ++i){ //checks if shape collides with placed blocks
            landed |= !!(permanent_board.board_bitarray[i] & test_board.board_bitarray[i]);
        }

        if (landed){ //if block has landed, writes it to permanent_board
            for (int i = 0; i < BITARRAY_SIZE; ++i){
                permanent_board.board_bitarray[i] |= temporary_board.board_bitarray[i];
            }
            new_score += RemoveFullRows();
        }
        else {
            crossarray_line_shift(left, &temporary_board);
            ++current_shape.x_pos;
        }
    }

    if (new_score){
        score += new_score * 100;
        time_diff /= new_score;
    }
    return landed;
}

/**
 * @brief Function generates new shape and places it to temporary_board
 * @return False if there is not enough space for placing new block, true otherwise
*/
bool PlaceNewShape(){
    current_shape.type = rand() % 7;
    current_shape.num_repr = shapes[current_shape.type][0].num_repr;
    current_shape.rotation = 0;
    memset(&temporary_board.board_bitarray, 0, sizeof(uint64_t) * 2);
    current_shape.x_pos = DEFAULT_POS;
    current_shape.y_pos = rand() % (COLS - DEFAULT_POS - 1) + DEFAULT_POS;

    current_shape.num_repr <<= current_shape.y_pos - DEFAULT_POS;
    if (current_shape.num_repr & permanent_board.board_bitarray[0]){
        return false;//if new block cannot fit in board
    }

    temporary_board.board_bitarray[0] |= current_shape.num_repr;
    return true;
}

/**
 * @brief Function handles rotation of block
*/
void Rotate(){
    if (current_shape.type == square || current_shape.x_pos + 1 == ROWS || 
        current_shape.y_pos < 2 || current_shape.y_pos > (7 - (current_shape.type == row))){
        return; //rotation would cause collision with right wall or shape is not rotable
    }

    int new_rotation = current_shape.rotation == 3 ? 0 : current_shape.rotation + 1;

    //if there is no collision with existing blocks, shape can be rotated
    if (!((permanent_board.board_rows[current_shape.x_pos - 2] & (shapes[current_shape.type][new_rotation].arr_repr[0] << current_shape.y_pos - DEFAULT_POS)) ||
        (permanent_board.board_rows[current_shape.x_pos - 1] & (shapes[current_shape.type][new_rotation].arr_repr[1] << current_shape.y_pos - DEFAULT_POS)) ||
        (permanent_board.board_rows[current_shape.x_pos] & (shapes[current_shape.type][new_rotation].arr_repr[2] << current_shape.y_pos - DEFAULT_POS)) ||
        (permanent_board.board_rows[current_shape.x_pos + 1] & (shapes[current_shape.type][new_rotation].arr_repr[3] << current_shape.y_pos - DEFAULT_POS)))){

        current_shape.rotation = new_rotation;
        current_shape.num_repr = shapes[current_shape.type][current_shape.rotation].num_repr;
        temporary_board.board_rows[current_shape.x_pos - 2] = current_shape.arr_repr[0] << current_shape.y_pos - DEFAULT_POS;
        temporary_board.board_rows[current_shape.x_pos - 1] = current_shape.arr_repr[1] << current_shape.y_pos - DEFAULT_POS;
        temporary_board.board_rows[current_shape.x_pos] = current_shape.arr_repr[2] << current_shape.y_pos - DEFAULT_POS;
        temporary_board.board_rows[current_shape.x_pos + 1] = current_shape.arr_repr[3] << current_shape.y_pos - DEFAULT_POS;

    }

}

/**
 * @brief Function for moving the shape
 * @param input Input from keyboard
 * @return True if block has landed, false otherwise
*/
bool Move(char input){
    bool blocked = false;
    switch (input){
        case 'a':
            for (int i = 0; i < BITARRAY_SIZE; ++i){
                blocked |= ((temporary_board.board_bitarray[i] & first_col_mask) || ((temporary_board.board_bitarray[i] >> 1) & permanent_board.board_bitarray[i]));
            } //if current_shape reached right wall, or the blocks in board, do not move

            if (!blocked){
                for (int i = 0; i < BITARRAY_SIZE; ++i){
                    temporary_board.board_bitarray[i] >>= 1;
                }
                --current_shape.y_pos;
            }
            break;

        case 's':
            return Descend();

        case 'd':
            for (int i = 0; i < BITARRAY_SIZE; ++i){
                blocked |= ((temporary_board.board_bitarray[i] & last_col_mask) || ((temporary_board.board_bitarray[i] << 1) & permanent_board.board_bitarray[i]));
                } //if current_shape reached right wall, or the blocks in board, do not move

            if (!blocked){
                for (int i = 0; i < BITARRAY_SIZE; ++i){
                    temporary_board.board_bitarray[i] <<= 1;
                }
                ++current_shape.y_pos;
            }
            break;
            
        case 'w':
            Rotate();
            break;
    }
    return false;
}

/**
 * @brief Function prints game board
*/
void PrintTable(bool game_is_running){
    char buffer[ROWS * (COLS * 2 + 1) + 35] = {0};
    int index = 0;
    index = sprintf(buffer, "\nTetris boadr!\n");
    clear();
    
    for (int i = 0; i < ROWS; ++i){
        uint8_t mask = 1;
        uint8_t current_row = permanent_board.board_rows[i] | temporary_board.board_rows[i];
        //int j = 0;
        //char output[COLS * 2 + 2] = {0};
        //output[COLS * 2] = '\n';
        
        while (mask){
            //output[j] = (current_row & mask) ? '#' : '.';
            //output[j + 1] = ' ';
            buffer[index] = (current_row & mask) ? '#' : '.';
            buffer[index + 1] = ' ';
            //j += 2;
            index += 2;
            mask <<= 1;
        }
        buffer[index++] = '\n';
        //index += sprintf(buffer + index, output);
    }
    index += sprintf(buffer + index, "Score: %d\n", score);
    game_is_running ? printw(buffer) : printf(buffer);
}

/**
 * @brief Function checks time difference
 * @param t1 Latter time value
 * @param t2 First time value
 * @param max_diff Max difference between time values
 * @return True if difference is bigger than max_diff
*/
inline static bool has_to_move(struct timeval* t1, struct timeval *t2, uint32_t max_diff){
    return (t1->tv_sec * 1000000 + t1->tv_usec) - (t2->tv_sec * 1000000 + t2->tv_usec) > max_diff;
}

//Main game loop
int main(){
    initscr();
    timeout(1);
    srand(time(0));

    bool game_is_running = true;
    struct timeval before_current_move = {0};
    struct timeval after_last_move = {0};

    char input = getch();
    PlaceNewShape();
    PrintTable(game_is_running);

    while (game_is_running){

        gettimeofday(&before_current_move, NULL);
        if ((input = getch()) != -1 || has_to_move(&before_current_move, &after_last_move, time_diff)){
            if (Move(input != -1 ? input : 's')){
                game_is_running = PlaceNewShape();
            }
            PrintTable(game_is_running);
            gettimeofday(&after_last_move, NULL);
        }
    }
    endwin();
    printf("\nGame over!\n");
    printf("Score: %d\n", score);
    return 0;
}