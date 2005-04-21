#include <stdio.h>
#include <arpa/nameser.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <resolver.h>
#include <res_errors.h>
#include <support.h>

#include "val_support.h"
#include "res_squery.h"
#include "validator.h"
#include "val_x_query.h"

#define AUTH_ZONE_INFO "fruits.netsec.tislabs.com"

// NOT AUTHORITATIVE FOR DS IN PARENT
#define NAME_SERVER_STRING	"158.69.82.20"
#define QUERY_NAME "dns.wesh.fruits.netsec.tislabs.com."
#define QUERY_TYPE ns_t_a
#define QUERY_CLASS ns_c_in

// QUERYING A RECURSIVE SERVER
/*
#define NAME_SERVER_STRING	"168.150.236.43"
#define QUERY_NAME "dns.wesh.fruits.netsec.tislabs.com."
#define QUERY_TYPE ns_t_a
#define QUERY_CLASS ns_c_in
*/

int init_respol(struct res_policy *respol)
{
	struct sockaddr_in *my_addr;
	struct in_addr  address;
	struct name_server *ns;
	char name_server_string[] = NAME_SERVER_STRING;
	char auth_zone_info[] = AUTH_ZONE_INFO;

	if(respol == NULL) 
		return SR_CALL_ERROR;

	ns = (struct name_server *) MALLOC (sizeof(struct name_server));
	if (ns == NULL)
		return SR_MEMORY_ERROR;

	respol->ns = ns;
	respol->ns->ns_name_n = (u_int8_t *) MALLOC (strlen(auth_zone_info) + 1);
	if(respol->ns->ns_name_n == NULL) 
		return SR_MEMORY_ERROR;
	memset(respol->ns->ns_name_n, 0, strlen(auth_zone_info) + 1);
	/* Initialize the rest of the fields */
	respol->ns->ns_tsig_key = NULL;
	respol->ns->ns_security_options = ZONE_USE_NOTHING;
	respol->ns->ns_status = 0;
	respol->ns->ns_next = NULL;
	respol->ns->ns_number_of_addresses = 1;
	if (inet_aton (name_server_string, &address)==0)
		return SR_INTERNAL_ERROR;
	my_addr = (struct sockaddr_in *) MALLOC (sizeof (struct sockaddr_in));
	if (my_addr == NULL) 
		return SR_MEMORY_ERROR;
	my_addr->sin_family = AF_INET;         // host byte order
	my_addr->sin_port = htons(53);     // short, network byte order
	my_addr->sin_addr = address;
	memcpy(respol->ns->ns_address, my_addr, sizeof(struct sockaddr));

	return SR_UNSET;
}

void destroy_respol(struct res_policy *respol)
{
	if (respol) free_name_servers(&respol->ns);
}

int main()
{

	char *name = QUERY_NAME;
	const u_int16_t type = QUERY_TYPE;
	const u_int16_t class = QUERY_CLASS;

	int ret_val;
	ret_val = val_x_query( NULL, name, type, class, 0, NULL, 0);

	return ret_val;
}
