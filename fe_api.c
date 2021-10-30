#include "fe.h"
#include "cel7ce.h"

static fe_Object *
fe_divide(fe_Context *ctx, fe_Object *arg)
{
	float accm = 0;
	float b;

	accm = fe_tonumber(ctx, fe_nextarg(ctx, &arg));

	do {
		b = fe_tonumber(ctx, fe_nextarg(ctx, &arg));
		accm /= b;
	} while (fe_type(ctx, arg) == FE_TPAIR);

	return fe_number(ctx, (float)round(accm));
}

static fe_Object *
fe_modulus(fe_Context *ctx, fe_Object *arg)
{
	ssize_t a = (ssize_t)fe_tonumber(ctx, fe_nextarg(ctx, &arg));
	ssize_t b = (ssize_t)fe_tonumber(ctx, fe_nextarg(ctx, &arg));
	return fe_number(ctx, (float)(a % b));
}

static fe_Object *
fe_quit(fe_Context *ctx, fe_Object *arg)
{
	UNUSED(arg);
	quit = true;
	return fe_bool(ctx, 0);
}

static fe_Object *
fe_rand(fe_Context *ctx, fe_Object *arg)
{
	ssize_t n = (ssize_t)fe_tonumber(ctx, fe_nextarg(ctx, &arg));
	return fe_number(ctx, (float)(rand() % n));
}

static fe_Object *
fe_poke(fe_Context *ctx, fe_Object *arg)
{
	size_t addr = (size_t)fe_tonumber(ctx, fe_nextarg(ctx, &arg));
	fe_Object *payload = fe_nextarg(ctx, &arg);

	if (fe_type(ctx, payload) == FE_TSTRING) {
		char buf[4096] = {0};
		size_t sz = fe_tostring(ctx, payload, (char *)&buf, sizeof(buf));
		memcpy(&memory[addr], (char *)&buf, sz);
	} else {
		size_t byte = (uint8_t)fe_tonumber(ctx, payload);
		memory[addr] = byte;
	}

	return fe_bool(ctx, 0);
}

static fe_Object *
fe_peek(fe_Context *ctx, fe_Object *arg)
{
	size_t addr = (size_t)fe_tonumber(ctx, fe_nextarg(ctx, &arg));
	size_t size = 1;

	if (fe_type(ctx, arg) == FE_TPAIR) {
		size = (size_t)fe_tonumber(ctx, fe_car(ctx, arg));

		char buf[4096] = {0};
		memcpy((void *)&buf, (void *)&memory[addr], size);

		return fe_string(ctx, (const char *)&buf);
	} else {
		return fe_number(ctx, (float)memory[addr]);
	}
}

static fe_Object *
fe_color(fe_Context *ctx, fe_Object *arg)
{
	color = (size_t)fe_tonumber(ctx, fe_nextarg(ctx, &arg));
	return fe_bool(ctx, 0);
}

static fe_Object *
fe_put(fe_Context *ctx, fe_Object *arg)
{
	char buf[4096] = {0};

	size_t sx = (size_t)fe_tonumber(ctx, fe_nextarg(ctx, &arg));
	size_t sy = (size_t)fe_tonumber(ctx, fe_nextarg(ctx, &arg));

	fe_Object *str = NULL;
	size_t x = sx;

	do {
		str = fe_nextarg(ctx, &arg);
		size_t sz = fe_tostring(ctx, str, (char *)&buf, sizeof(buf));

		for (size_t i = 0; i < sz && x < config.width; ++i, ++x) {
			size_t coord = sy * config.width + x;
			size_t addr = DISPLAY_START + (coord * 2);
			memory[addr + 0] = buf[i];
			memory[addr + 1] = color;
		}

	} while (fe_type(ctx, arg) == FE_TPAIR);

	return fe_bool(ctx, 0);
}

static fe_Object *
fe_get(fe_Context *ctx, fe_Object *arg)
{
	size_t x = (size_t)fe_tonumber(ctx, fe_nextarg(ctx, &arg));
	size_t y = (size_t)fe_tonumber(ctx, fe_nextarg(ctx, &arg));
	uint8_t res = memory[y * config.width + x + 0];
	return fe_number(ctx, res);
}

static fe_Object *
fe_fill(fe_Context *ctx, fe_Object *arg)
{
	size_t x = (size_t)fe_tonumber(ctx, fe_nextarg(ctx, &arg));
	size_t y = (size_t)fe_tonumber(ctx, fe_nextarg(ctx, &arg));
	size_t w = (size_t)fe_tonumber(ctx, fe_nextarg(ctx, &arg));
	size_t h = (size_t)fe_tonumber(ctx, fe_nextarg(ctx, &arg));

	char buf[2] = {0};
	size_t sz = fe_tostring(ctx, fe_nextarg(ctx, &arg), (char *)&buf, sizeof(buf));
	if (sz < 1 || sz > 1) {
		fe_error(ctx, "Expected a string with one character");
	}
	size_t c = buf[0];

	for (size_t dy = y; dy < (y + h); ++dy) {
		for (size_t dx = x; dx < (x + w); ++dx) {
			size_t coord = dy * config.width + dx;
			size_t addr = DISPLAY_START + (coord * 2);
			memory[addr + 0] = c;
			memory[addr + 1] = color;
		}
	}

	return fe_bool(ctx, 0);
}

static fe_Object *
fe_strlen(fe_Context *ctx, fe_Object *arg)
{
	static char buf[4096] = {0};
	size_t sz = fe_tostring(ctx, fe_nextarg(ctx, &arg), (char *)&buf, sizeof(buf));
	return fe_number(ctx, (float)sz);
}

static fe_Object *
fe_strstart(fe_Context *ctx, fe_Object *arg)
{
	static char buf1[4096] = {0};
	fe_tostring(ctx, fe_nextarg(ctx, &arg), (char *)&buf1, sizeof(buf1));
	static char buf2[4096] = {0};
	size_t sz = fe_tostring(ctx, fe_nextarg(ctx, &arg), (char *)&buf2, sizeof(buf2));
	return fe_bool(ctx, !strncmp((const char *)&buf1, (const char *)&buf2, sz));
}

static fe_Object *
fe_strat(fe_Context *ctx, fe_Object *arg)
{
	static char buf[4096] = {0};
	fe_tostring(ctx, fe_nextarg(ctx, &arg), (char *)&buf, sizeof(buf));
	size_t ind = (size_t)fe_tonumber(ctx, fe_nextarg(ctx, &arg));
	buf[ind + 1] = '\0';
	return fe_string(ctx, (const char *)&buf[ind]);
}

static fe_Object *
fe_ch2num(fe_Context *ctx, fe_Object *arg)
{
	static char buf[2] = {0};
	fe_tostring(ctx, fe_nextarg(ctx, &arg), (char *)&buf, sizeof(buf));
	return fe_number(ctx, (float)buf[0]);
}

static fe_Object *
fe_num2ch(fe_Context *ctx, fe_Object *arg)
{
	uint8_t num = (uint8_t)fe_tonumber(ctx, fe_nextarg(ctx, &arg));
	char buf[2] = {num, 0};
	return fe_string(ctx, buf);
}

static fe_Object *
fe_username(fe_Context *ctx, fe_Object *arg)
{
	UNUSED(arg);
	return fe_string(ctx, get_username());
}

const struct ApiFunc fe_apis[16] = {
	{        "//",    fe_divide },
	{         "%",   fe_modulus },
	{      "quit",      fe_quit },
	{      "rand",      fe_rand },
	{      "poke",      fe_poke },
	{      "peek",      fe_peek },
	{     "color",     fe_color },
	{       "put",       fe_put },
	{       "get",       fe_get },
	{      "fill",      fe_fill },
	{    "strlen",    fe_strlen },
	{  "strstart",  fe_strstart },
	{     "strat",     fe_strat },
	{ "char->num",    fe_ch2num },
	{ "num->char",    fe_num2ch },
	{  "username",  fe_username },
};
