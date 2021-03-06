#include <math.h>
#include <sys/time.h>

#include "cel7ce.h"
#include "janet.h"

// Internal-only APIs. There are no fe variants of these, as the internal
// scripts are written in Janet only.

static Janet
janet_lderr(int32_t argc, Janet *argv)
{
	UNUSED(argv);
	janet_fixarity(argc, 0);
	return janet_wrap_boolean(load_error);
}

static Janet
janet_swimd(int32_t argc, Janet *argv)
{
	janet_fixarity(argc, 1);
	mode.cur = (size_t)janet_getnumber(argv, 0);
	// TODO: validate input
	return janet_wrap_nil();
}

// ----------------------------------------------------------------------------
// Public APIs.

static Janet
janet_idivide(int32_t argc, Janet *argv)
{
	janet_arity(argc, 2, -1);

	double accm = janet_getnumber(argv, 0);

	for (size_t i = 1; i < (size_t)argc; ++i) {
		double arg = janet_getnumber(argv, i);
		accm /= arg;
	}

	return janet_wrap_number(round(accm));
}

static Janet
janet_quit(int32_t argc, Janet *argv)
{
	janet_fixarity(argc, 0);
	UNUSED(argv);
	quit = true;
	return janet_wrap_nil();
}

static Janet
janet_rand(int32_t argc, Janet *argv)
{
	janet_fixarity(argc, 1);
	size_t n = (size_t)janet_getnumber(argv, 0);

	if (n == 0) {
		janet_panicf("Expected non-zero argument.");
	}

	return janet_wrap_number((double)(rand() % n));
}

static Janet
janet_poke(int32_t argc, Janet *argv)
{
	janet_fixarity(argc, 2);

	size_t addr = (size_t)janet_getnumber(argv, 0);

	if (janet_checktype(argv[1], JANET_STRING)) {
		JanetString str = janet_getstring(argv, 1);
		check_user_address(LM_Janet, addr, janet_string_length(str), true);
		memcpy(&memory[bank][addr], str, janet_string_length(str));
	} else if (janet_checktype(argv[1], JANET_NUMBER)) {
		check_user_address(LM_Janet, addr, 1, true);
		size_t byte = (uint8_t)janet_getnumber(argv, 1);
		memory[bank][addr] = byte;
	} else {
		janet_panicf("bad slot #1, expected %T or %T, got %v",
			JANET_STRING, JANET_NUMBER, argv[1]);
	}

	return janet_wrap_nil();
}

static Janet
janet_peek(int32_t argc, Janet *argv)
{
	janet_arity(argc, 1, 2);

	size_t addr = (size_t)janet_getnumber(argv, 0);
	size_t size = (size_t)janet_optnumber(argv, argc, 1, 1);

	if (size > 1) {
		check_user_address(LM_Janet, addr, size, false);

		char *buf = janet_smalloc(size);
		memset(buf, 0x0, size);
		memcpy(buf, (void *)&memory[bank][addr], size);

		return janet_stringv((uint8_t *)buf, size);
	} else {
		check_user_address(LM_Janet, addr, 1, false);
		return janet_wrap_number((double)memory[bank][addr]);
	}
}

static Janet
janet_color(int32_t argc, Janet *argv)
{
	janet_fixarity(argc, 1);
	color = (uint8_t)janet_getnumber(argv, 0);
	return janet_wrap_nil();
}

static Janet
janet_cel7put(int32_t argc, Janet *argv)
{
	janet_arity(argc, 3, -1);

	if (bank == BK_Rom) {
		janet_panicf("Cannot write to bank.");
	}

	size_t sx = (size_t)janet_getnumber(argv, 0);
	size_t sy = (size_t)janet_getnumber(argv, 1);

	for (size_t x = sx, arg = 2; arg < (size_t)argc; ++arg) {
		char *str = (char *)janet_getstring(argv, arg);
		size_t sz = strlen(str);

		for (size_t i = 0; i < sz && x < config.width; ++i, ++x) {
			size_t coord = sy * config.width + x;
			size_t addr = DISPLAY_START + (coord * 2);
			memory[BK_Normal][addr + 0] = str[i];
			memory[BK_Normal][addr + 1] = color;
		}
	}

	return janet_wrap_nil();
}

static Janet
janet_cel7get(int32_t argc, Janet *argv)
{
	janet_fixarity(argc, 2);

	size_t x = (size_t)janet_getnumber(argv, 0);
	size_t y = (size_t)janet_getnumber(argv, 1);

	uint8_t res = memory[BK_Normal][y * config.width + x + 0];
	return janet_wrap_number((double)res);
}

static Janet
janet_fill(int32_t argc, Janet *argv)
{
	janet_fixarity(argc, 5);

	if (bank == BK_Rom) {
		janet_panicf("Cannot write to bank.");
	}

	size_t x = (size_t)janet_getnumber(argv, 0);
	size_t y = (size_t)janet_getnumber(argv, 1);
	size_t w = (size_t)janet_getnumber(argv, 2);
	size_t h = (size_t)janet_getnumber(argv, 3);

	const uint8_t *str = janet_getstring(argv, 4);
	if (strlen((char *)str) != 1)
		janet_panicf("bad slot #5, expected a string with one character");
	size_t c = str[0];

	for (size_t dy = y; dy < (y + h); ++dy) {
		for (size_t dx = x; dx < (x + w); ++dx) {
			size_t coord = dy * config.width + dx;
			size_t addr = DISPLAY_START + (coord * 2);
			memory[BK_Normal][addr + 0] = c;
			memory[BK_Normal][addr + 1] = color;
		}
	}

	return janet_wrap_nil();
}

static Janet
janet_username(int32_t argc, Janet *argv)
{
	janet_fixarity(argc, 0);
	UNUSED(argv);

	const uint8_t *u = (const uint8_t *)get_username();
	return janet_stringv(u, strlen((char *)u));
}

static Janet
janet_delay(int32_t argc, Janet *argv)
{
	janet_fixarity(argc, 1);
	double delay = janet_getnumber(argv, 0);

	if (delay < 0 || !isnormal(delay)) {
		janet_panicf("Delay %f invalid.", delay);
	}

	delay_val.tv_sec  = (time_t)round(delay);
	delay_val.tv_usec = (suseconds_t)((delay - round(delay)) * 1000000);
	gettimeofday(&delay_set, NULL);

	return janet_wrap_nil();
}

static Janet
janet_ticks(int32_t argc, Janet *argv)
{
	janet_fixarity(argc, 0);
	UNUSED(argv);
	return janet_wrap_number((double)mode.steps[mode.cur]);
}

static Janet
janet_swibnk(int32_t argc, Janet *argv)
{
	janet_fixarity(argc, 1);

	double bank_arg = janet_getnumber(argv, 0);

	if (bank_arg < 0 || bank_arg > BK_COUNT) {
		janet_panicf("Cannot switch to bank %.f.", bank_arg);
	}

	bank = (size_t)bank_arg;

	return janet_wrap_nil();
}

const struct JanetReg janet_apis[16] = {
	{     "lderr",    janet_lderr, "" },
	{     "swimd",    janet_swimd, "" },
	{        "//",  janet_idivide, "" },
	{      "quit",     janet_quit, "" },
	{      "rand",     janet_rand, "" },
	{      "poke",     janet_poke, "" },
	{      "peek",     janet_peek, "" },
	{     "color",    janet_color, "" },
	{     "c7put",  janet_cel7put, "" },
	{     "c7get",  janet_cel7get, "" },
	{      "fill",     janet_fill, "" },
	{  "username", janet_username, "" },
	{     "delay",    janet_delay, "" },
	{     "ticks",    janet_ticks, "" },
	{    "swibnk",   janet_swibnk, "" },

	// Include a null sentinel, because janet_cfunc is too braindamaged
	// to take a "sz" parameter.
	//
	// The first time I wrote this code I forgot the sentinel value, as
	// that whole class of functions weren't documented at the time.
	// I noticed the issue when I started getting segfaults in the fe_
	// versions of these functions, and was like... how the hell are the
	// fe_* functions being called from Janet code.
	//
	// Long live C~
	{        NULL,           NULL, "" },
};
