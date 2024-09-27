#pragma once

#include <device/hal.h>
#include <device/board.h>
#include <option/has_mmu2.h>
#include <option/has_puppies.h>

#if BOARD_IS_BUDDY()
extern UART_HandleTypeDef uart_handle_for_tmc;
void uart_init_tmc();
#endif

#if HAS_PUPPIES()
extern UART_HandleTypeDef uart_handle_for_puppies;
void uart_init_puppies();
#endif

#if HAS_MMU2()
extern UART_HandleTypeDef uart_handle_for_mmu;
void uart_init_mmu();
#endif

extern UART_HandleTypeDef uart_handle_for_esp;
void uart_init_esp();
