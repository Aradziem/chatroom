#include "connection.h"

void network_data::connection::write_read(int w_sz, void *w_buf, int r_sz, void *r_buf)
{
	write(w_sz, w_buf);
	read(r_sz, r_buf);
}

