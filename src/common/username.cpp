#include "username.h"

#include <string.h>

static char const b64[64] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789"
	"-"/* \0 */;
static constexpr uint8_t sentinel_idx = 63;

static uint8_t to_id(char const c);
static char from_id(unsigned int const id);
static void pack(uint8_t p[3], char const s[4]);
static void unpack(char s[4], uint8_t const p[3]);
static std::string username_chunk(struct username const un, unsigned int const chunk);

static uint8_t to_id(char const c)
{
	unsigned int i;

	for(i = 0; i < 64; ++i) {
		if(b64[i] == c) return i;
	}
	return sentinel_idx;
}

static char from_id(unsigned int const id)
{
	if(id == sentinel_idx || id > 63) return b64[sentinel_idx];
	return b64[id];
}

static void pack(uint8_t p[3], char const s[4])
{
	p[0]  = to_id(s[0]);      /* ??h0000l ???????? ???????? */
	p[0] |= to_id(s[1]) << 6; /* 1lh0000l ???????? ???????? */
	p[1]  = to_id(s[1]) >> 2; /* 1lh0000l ????h111 ???????? */
	p[1] |= to_id(s[2]) << 4; /* 1lh0000l 222lh111 ???????? */
	p[2]  = to_id(s[2]) >> 4; /* 1lh0000l 222lh111 ??????h2 */
	p[2] |= to_id(s[3]) << 2; /* 1lh0000l 222lh111 h3333lh2 */
}

static void unpack(char s[4], uint8_t const p[3])
{
	s[0] = from_id( p[0] & 0b00111111);
	s[1] = from_id((p[0] & 0b11000000) >> 6 | (p[1] & 0b00001111) << 2);
	s[2] = from_id((p[1] & 0b11110000) >> 4 | (p[2] & 0b00000011) << 4);
	s[3] = from_id( p[2] >> 2);
}

struct username un_from_str(std::string const sv)
{
	struct username un;
	unsigned int i, j;
	char const *iter;
	char raw[8];

	iter = sv.data();
	for(i = 0; i < 8; ++i) {
		for(j = 0; j < 8 && (
					(*iter >= 'A' && *iter <= 'Z') ||
					(*iter >= 'a' && *iter <= 'z') ||
					(*iter >= '0' && *iter <= '9') ||
					*iter == '-' || *iter == '\0'); ++j, ++iter) {
			raw[j] = *iter;
		}

		memset(un.packed[i], 0, 6);
		memset(raw + j, 0, 8 - j);
		
		pack(un.packed[i] + 0, raw + 0);
		pack(un.packed[i] + 3, raw + 4);

		if(*iter == '/') ++iter;
	}

	return un;
}

static std::string username_chunk(struct username const un, unsigned int const chunk)
{
	std::string str;

	str.resize(8);
	unpack(str.data() + 0, un.packed[chunk] + 0);
	unpack(str.data() + 4, un.packed[chunk] + 3);

	return str;
}

std::string username_pretty(struct username const un)
{
	std::string str;
	unsigned int i;

	str = username_chunk(un, 0);
	for(i = 1; i < 8; ++i) {
		str += '/';
		str += username_chunk(un, i);
	}

	return str;
}

