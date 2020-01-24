// wizard_types.h
#ifndef _WIZARD_TYPES
#define _WIZARD_TYPES

#include <assert.h>

typedef enum {
    _STATE_START,
    _STATE_INIT,
    _STATE_INFO,
    _STATE_FIRST,

    _STATE_SELFTEST_INIT,
    _STATE_SELFTEST_FAN0,
    _STATE_SELFTEST_FAN1,
    _STATE_SELFTEST_X,
    _STATE_SELFTEST_Y,
    _STATE_SELFTEST_Z,
    _STATE_SELFTEST_COOL,
    _STATE_SELFTEST_INIT_TEMP,
    _STATE_SELFTEST_TEMP,
    _STATE_SELFTEST_PASS,
    _STATE_SELFTEST_FAIL,

    _STATE_XYZCALIB_INIT,
    _STATE_XYZCALIB_HOME,
    _STATE_XYZCALIB_Z,
    _STATE_XYZCALIB_XY_MSG_CLEAN_NOZZLE,
    _STATE_XYZCALIB_XY_MSG_IS_SHEET,
    _STATE_XYZCALIB_XY_MSG_REMOVE_SHEET,
    _STATE_XYZCALIB_XY_MSG_PLACE_PAPER,
    _STATE_XYZCALIB_XY_SEARCH,
    _STATE_XYZCALIB_XY_MSG_PLACE_SHEET,
    _STATE_XYZCALIB_XY_MEASURE,
    _STATE_XYZCALIB_PASS,
    _STATE_XYZCALIB_FAIL,

    _STATE_FIRSTLAY_INIT,
    _STATE_FIRSTLAY_LOAD,
    _STATE_FIRSTLAY_MSBX_CALIB,
    _STATE_FIRSTLAY_MSBX_START_PRINT,
    _STATE_FIRSTLAY_PRINT,
    _STATE_FIRSTLAY_MSBX_REPEAT_PRINT,
    _STATE_FIRSTLAY_FAIL,
    _STATE_FINISH,

    _STATE_LAST
} wizard_state_t;

static_assert(_STATE_LAST < 64, "too many states in wizard_state_t");

#define _STATE_MASK(state) (((uint64_t)1) << state)

#define _STATE_MASK_WIZARD_START ( \
    _STATE_MASK(_STATE_START) | _STATE_MASK(_STATE_INIT) | _STATE_MASK(_STATE_INFO) | _STATE_MASK(_STATE_FIRST))

#define _STATE_MASK_SELFTEST ( \
    _STATE_MASK(_STATE_SELFTEST_INIT) | _STATE_MASK(_STATE_SELFTEST_FAN0) | _STATE_MASK(_STATE_SELFTEST_FAN1) | _STATE_MASK(_STATE_SELFTEST_X) | _STATE_MASK(_STATE_SELFTEST_Y) | _STATE_MASK(_STATE_SELFTEST_Z) | _STATE_MASK(_STATE_SELFTEST_COOL) | _STATE_MASK(_STATE_SELFTEST_INIT_TEMP) | _STATE_MASK(_STATE_SELFTEST_TEMP) | _STATE_MASK(_STATE_SELFTEST_PASS) | _STATE_MASK(_STATE_SELFTEST_FAIL) | _STATE_MASK(_STATE_LAST))

#define _STATE_MASK_XYZCALIB ( \
    _STATE_MASK(_STATE_XYZCALIB_INIT) | _STATE_MASK(_STATE_XYZCALIB_HOME) | _STATE_MASK(_STATE_XYZCALIB_Z) | _STATE_MASK(_STATE_XYZCALIB_XY_MSG_CLEAN_NOZZLE) | _STATE_MASK(_STATE_XYZCALIB_XY_MSG_IS_SHEET) | _STATE_MASK(_STATE_XYZCALIB_XY_MSG_REMOVE_SHEET) | _STATE_MASK(_STATE_XYZCALIB_XY_MSG_PLACE_PAPER) | _STATE_MASK(_STATE_XYZCALIB_XY_SEARCH) | _STATE_MASK(_STATE_XYZCALIB_XY_MSG_PLACE_SHEET) | _STATE_MASK(_STATE_XYZCALIB_XY_MEASURE) | _STATE_MASK(_STATE_XYZCALIB_PASS) | _STATE_MASK(_STATE_XYZCALIB_FAIL) | _STATE_MASK(_STATE_LAST))

#define _STATE_MASK_FIRSTLAY ( \
    _STATE_MASK(_STATE_FIRSTLAY_INIT) | _STATE_MASK(_STATE_FIRSTLAY_LOAD) | _STATE_MASK(_STATE_FIRSTLAY_MSBX_CALIB) | _STATE_MASK(_STATE_FIRSTLAY_MSBX_START_PRINT) | _STATE_MASK(_STATE_FIRSTLAY_PRINT) | _STATE_MASK(_STATE_FIRSTLAY_MSBX_REPEAT_PRINT) | _STATE_MASK(_STATE_FIRSTLAY_FAIL) | _STATE_MASK(_STATE_LAST))
/*
#define _STATE_MASK_WIZARD ( \
	_STATE_MASK_WIZARD_START | \
	_STATE_MASK_SELFTEST | \
	_STATE_MASK_XYZCALIB | \
	_STATE_MASK_FIRSTLAY | \
	_STATE_MASK(_STATE_FINISH) | \
	_STATE_MASK(_STATE_LAST) )*/

//disabled XYZ
#define _STATE_MASK_WIZARD (_STATE_MASK_WIZARD_START | _STATE_MASK_SELFTEST | _STATE_MASK_FIRSTLAY | _STATE_MASK(_STATE_FINISH) | _STATE_MASK(_STATE_LAST))

#define _SCREEN_NONE 0
#define _SCREEN_SELFTEST_FANS_XYZ 1
#define _SCREEN_SELFTEST_TEMP 2
#define _SCREEN_XYZCALIB_HOME 3

typedef enum {
    _TEST_START,
    _TEST_RUN,
    _TEST_PASSED,
    _TEST_FAILED
} _TEST_STATE_t;

static inline int _is_test_done(int result) {
    return (result == _TEST_PASSED) || (result == _TEST_FAILED);
}

#endif //_SCREEN_WIZARD
