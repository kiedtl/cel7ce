#include "cel7ce.h"
#include "janet.h"

static Janet
janet_quit(int32_t argc, Janet *argv)
{
	janet_fixarity(argc, 0);
	UNUSED(argv);
	quit = true;
	return janet_wrap_nil();
}

static Janet
janet_poke(int32_t argc, Janet *argv)
{
	janet_fixarity(argc, 2);

	size_t addr = (size_t)janet_getnumber(argv, 0);
	Janet payload = argv[1];

	if (janet_checktype(payload, JANET_STRING)) {
		char *str = (char *)janet_unwrap_string(payload);
		memcpy(&memory[addr], str, strlen(str));
	} else if (janet_checktype(payload, JANET_NUMBER)) {
		size_t byte = (uint8_t)janet_unwrap_number(payload);
		memory[addr] = byte;
	} else {
		janet_panicf("bad slot #1, expected %T or %T, got %v",
			JANET_STRING, JANET_NUMBER, payload);
	}

	return janet_wrap_nil();
}

static Janet
janet_peek(int32_t argc, Janet *argv)
{
	janet_arity(argc, 1, 2);

	size_t addr = (size_t)janet_getnumber(argv, 0);
	size_t size = (size_t)janet_optnumber(argv, argc, 0, 1);

	if (size > 1) {
		char buf[4096] = {0};
		memcpy((void *)&buf, (void *)&memory[addr], size);

		return janet_stringv((uint8_t *)&buf, size);
	} else {
		return janet_wrap_number((double)memory[addr]);
	}
}

static Janet
janet_color(int32_t argc, Janet *argv)
{
	janet_fixarity(argc, 1);
	color = (size_t)janet_getnumber(argv, 0);
	return janet_wrap_nil();
}

static Janet
janet_cel7put(int32_t argc, Janet *argv)
{
	janet_arity(argc, 3, -1);

	size_t sx = (size_t)janet_getnumber(argv, 0);
	size_t sy = (size_t)janet_getnumber(argv, 1);

	for (size_t x = sx, arg = 2; arg < (size_t)argc; ++arg) {
		char *str = (char *)janet_getstring(argv, arg);
		size_t sz = strlen(str);

		for (size_t i = 0; i < sz && x < config.width; ++i, ++x) {
			size_t coord = sy * config.width + x;
			size_t addr = DISPLAY_START + (coord * 2);
			memory[addr + 0] = str[i];
			memory[addr + 1] = color;
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

	uint8_t res = memory[y * config.width + x + 0];
	return janet_wrap_number((double)res);
}

static Janet
janet_fill(int32_t argc, Janet *argv)
{
	janet_fixarity(argc, 5);

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
			memory[addr + 0] = c;
			memory[addr + 1] = color;
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

const struct JanetReg janet_apis[9] = {
	{      "quit",     janet_quit, "" },
	{      "poke",     janet_poke, "" },
	{      "peek",     janet_peek, "" },
	{     "color",    janet_color, "" },
	{     "c7put",  janet_cel7put, "" },
	{     "c7get",  janet_cel7get, "" },
	{      "fill",     janet_fill, "" },
	{  "username", janet_username, "" },

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
