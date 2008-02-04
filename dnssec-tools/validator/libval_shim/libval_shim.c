#include <stdio.h>

#define __USE_GNU // This is needed for the RTLD_NEXT definition
#include <stdlib.h>
#include <dlfcn.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netinet/in.h>
#include <resolv.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/nameser.h>
#include <validator/resolver.h>
#include <validator/validator.h>

typedef struct val_context ValContext;

struct hostent *
gethostbyname(const char *name)
{
  int (*lib_gethostbyname)(const char *name);
  char *error;

  lib_gethostbyname = dlsym(RTLD_NEXT, "gethostbyname");

  if ((error = dlerror()) != NULL) {
    fprintf(stderr, "unable to load gethostbyname: %s\n", error);
    exit(1);
  }

  fprintf(stderr, "libval_shim: gethostbyname called: pass-thru\n");

  return (struct hostent *)lib_gethostbyname(name);
}



int
gethostbyname_r(__const char * __name,struct hostent * __result_buf, char * __buf, size_t __buflen, struct hostent ** __result, int * __h_errnop)
{
  int (*lib_gethostbyname_r)(__const char * __name,struct hostent * __result_buf, char * __buf, size_t __buflen, struct hostent ** __result, int * __h_errnop);
  char *error;

  lib_gethostbyname_r = dlsym(RTLD_NEXT, "gethostbyname_r");

  if ((error = dlerror()) != NULL) {
    fprintf(stderr, "unable to load gethostbyname_r: %s\n", error);
    exit(1);
  }

  fprintf(stderr, "libval_shim: gethostbyname_r called: pass-thru\n");

  return (int)lib_gethostbyname_r(__name, __result_buf, __buf, __buflen, __result, __h_errnop);
}



struct hostent *
gethostbyaddr(__const void *addr, __socklen_t len, int type)
{
  int (*lib_gethostbyaddr)(__const void *addr, __socklen_t len, int type);
  char *error;

  lib_gethostbyaddr = dlsym(RTLD_NEXT, "gethostbyaddr");

  if ((error = dlerror()) != NULL) {
    fprintf(stderr, "unable to load gethostbyaddr: %s\n", error);
    exit(1);
  }

  fprintf(stderr, "libval_shim: gethostbyaddr called: pass-thru\n");

  return (struct hostent *)lib_gethostbyaddr(addr, len, type);
}



struct hostent *
gethostbyname2(__const char *__name, int __af)
{
  int (*lib_gethostbyname2)(__const char *__name, int __af);
  char *error;

  lib_gethostbyname2 = dlsym(RTLD_NEXT, "gethostbyname2");

  if ((error = dlerror()) != NULL) {
    fprintf(stderr, "unable to load gethostbyname2: %s\n", error);
    exit(1);
  }

  fprintf(stderr, "libval_shim: gethostbyname2 called: pass-thru\n");

  return (struct hostent *)lib_gethostbyname2(__name, __af);
}



int
gethostbyname2_r(__const char * __name, int __af, struct hostent * __result_buf, char * __buf, size_t __buflen, struct hostent ** __result, int * __h_errnop)
{
  int (*lib_gethostbyname2_r)(__const char * __name, int __af, struct hostent * __result_buf, char * __buf, size_t __buflen, struct hostent ** __result, int * __h_errnop);
  char *error;

  lib_gethostbyname2_r = dlsym(RTLD_NEXT, "gethostbyname2_r");

  if ((error = dlerror()) != NULL) {
    fprintf(stderr, "unable to load gethostbyname2_r: %s\n", error);
    exit(1);
  }

  fprintf(stderr, "libval_shim: gethostbyname2_r called: pass-thru\n");

  return (int)lib_gethostbyname2_r(__name, __af, __result_buf, __buf, __buflen, __result, __h_errnop);
}



int
getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res)
{
  ValContext *		ctx = NULL;
  struct val_addrinfo *	vainfo_ptr = NULL;
  val_status_t          val_status;
  int ret;
  fprintf(stderr, "libval_shim: getaddrinfo(%s, %s) called: wrapper\n", node, service);

  ret = val_getaddrinfo(ctx, node, service, hints, &vainfo_ptr, &val_status);
  if (ret == 0) {
    *res = (struct addrinfo *)vainfo_ptr;
  }

  if (val_istrusted(val_status)) {
      return 0;
  }

  if (ret) {
    return ret;
  }

  return (EAI_NONAME); 
}



void
freeaddrinfo(struct addrinfo *ai)
{
  fprintf(stderr, "libval_shim: freeaddrinfo called: wrapper\n");

  val_freeaddrinfo((struct val_addrinfo *)ai);
}



int
getnameinfo(__const struct sockaddr * sa, socklen_t salen,char * host, socklen_t hostlen, char *serv, socklen_t servlen, unsigned int flags)
{
  ValContext *		ctx = NULL;
  val_status_t          val_status;
  char *addr;
  int ret;

  addr = inet_ntoa(((struct sockaddr_in*)sa)->sin_addr);
  fprintf(stderr, "libval_shim: getnameinfo(%s,%ld) called: wrapper\n", addr, ((struct sockaddr_in*)sa)->sin_port);

  ret = val_getnameinfo(ctx, sa, salen, host, hostlen, serv, servlen, flags,
			&val_status);

  if (val_istrusted(val_status)) {
      return 0;
  }

  if (ret) {
    return ret;
  }

  return (EAI_NONAME); 
}



int
res_init(void)
{
  int (*lib_res_init)(void);
  char *error;

  lib_res_init = dlsym(RTLD_NEXT, "res_init");

  if ((error = dlerror()) != NULL) {
    fprintf(stderr, "unable to load res_init: %s\n", error);
    exit(1);
  }

  fprintf(stderr, "libval_shim: res_init called: pass-thru\n");

  return (int)lib_res_init();
}



int
res_query(const char *dname, int class, int type, unsigned char *answer, int anslen)
{
  ValContext *		ctx = NULL;
  val_status_t          val_status;
  int ret;

  fprintf(stderr, "libval_shim: res_query(%s,%d,%d) called: wrapper\n",
	  dname, class, type);

  ret = val_res_query(ctx, dname, class, type, answer, anslen,
			&val_status);

  if (val_istrusted(val_status)) {
    return 0;
  }

  if (ret) {
    return ret;
  }

  return (-1); 
}



int
res_querydomain(const char *name, const char *domain, int class, int type, u_char * answer, int anslen)
{
  int (*lib_res_querydomain)(const char *name, const char *domain, int class, int type, u_char * answer, int anslen);
  char *error;

  lib_res_querydomain = dlsym(RTLD_NEXT, "res_querydomain");

  if ((error = dlerror()) != NULL) {
    fprintf(stderr, "unable to load res_querydomain: %s\n", error);
    exit(1);
  }

  fprintf(stderr, "libval_shim: res_querydomain called: pass-thru\n");

  return (int)lib_res_querydomain(name, domain, class, type, answer, anslen);
}



int
res_search(const char *dname, int class, int type, unsigned char *answer, int anslen)
{
  int (*lib_res_search)(const char *dname, int class, int type, unsigned char *answer, int anslen);
  char *error;

  lib_res_search = dlsym(RTLD_NEXT, "res_search");

  if ((error = dlerror()) != NULL) {
    fprintf(stderr, "unable to load res_search: %s\n", error);
    exit(1);
  }

  fprintf(stderr, "libval_shim: res_search called: pass-thru\n");

  return (int)lib_res_search(dname, class, type, answer, anslen);
}



int
res_send(const u_char * msg, int msglin, u_char *answer, int anslen)
{
  int (*lib_res_send)(const u_char * msg, int msglin, u_char *answer, int anslen);
  char *error;

  lib_res_send = dlsym(RTLD_NEXT, "res_send");

  if ((error = dlerror()) != NULL) {
    fprintf(stderr, "unable to load res_send: %s\n", error);
    exit(1);
  }

  fprintf(stderr, "libval_shim: res_send called: pass-thru\n");

  return (int)lib_res_send(msg, msglin, answer, anslen);
}



struct hostent *
getipnodebyname(const char *name, int af, int flags, int *error_num)
{
  // int (*lib_getipnodebyname)(const char *name, int af, int flags, int *error_num);
  // char *error;

  // lib_getipnodebyname = dlsym(RTLD_NEXT, "getipnodebyname");
  //
  // if ((error = dlerror()) != NULL) {
  //   fprintf(stderr, "unable to load getipnodebyname: %s\n", error);
  //   exit(1);
  // }

  fprintf(stderr, "libval_shim: getipnodebyname: called: not-avail\n");

  return (struct hostent *)NULL;
}


struct hostent *
getipnodebyaddr(const void *addr, size_t len, int af, int *error_num)
{
  // int (*lib_getipnodebyaddr)(const void *addr, size_t len, int af, int *error_num);
  // char *error;

  // lib_getipnodebyaddr = dlsym(RTLD_NEXT, "getipnodebyaddr");
  //
  // if ((error = dlerror()) != NULL) {
  //   fprintf(stderr, "unable to load getipnodebyaddr: %s\n", error);
  //   exit(1);
  // }

  fprintf(stderr, "libval_shim: getipnodebyaddr: called: not-avail\n");

  return (struct hostent *)NULL;
}


/* int
getrrsetbyname(const char *hostname, unsigned int rdclass, unsigned int rdtype, unsigned int flags, struct rrsetinfo **res)
{
  // int (*lib_getrrsetbyname)(const char *hostname, unsigned int rdclass, unsigned int rdtype, unsigned int flags, struct rrsetinfo **res);
  // char *error;

  // lib_getrrsetbyname = dlsym(RTLD_NEXT, "getrrsetbyname");
  //
  // if ((error = dlerror()) != NULL) {
  //   fprintf(stderr, "unable to load getrrsetbyname: %s\n", error);
  //   exit(1);
  // }

  fprintf(stderr, "libval_shim: getrrsetbyname: called: not-avail\n");

  return (int)NULL;
}
*/

