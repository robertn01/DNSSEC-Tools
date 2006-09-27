/*
 * Copyright 2005 SPARTA, Inc.  All rights reserved.
 * See the COPYING file distributed with this software for details.
 *
 * Author: Abhijit Hayatnagarkar
 *
 * This is the implementation file for a validating getaddrinfo function.
 * Applications should be able to use this in place of getaddrinfo with
 * minimal change.
 */
#include "validator-config.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>
#include <resolv.h>

#include <validator.h>
#include <resolver.h>
#include "val_policy.h"
#include "val_log.h"


/*
 * Function: append_val_addrinfo
 *
 * Purpose: A utility function to link one val_addrinfo linked list to another.
 *          The scope of this function is limited to this file.
 *
 * Parameters:
 *             a1 -- A pointer to the first val_addrinfo linked list
 *             a2 -- A pointer to the second val_addrinfo linked list
 *
 * Returns:
 *             a2 appended to a1.
 */
static struct val_addrinfo *
append_val_addrinfo(struct val_addrinfo *a1, struct val_addrinfo *a2)
{
    struct val_addrinfo *a;
    if (a1 == NULL)  
        return a2;
    if (a2 == NULL)  
        return a1;

    a = a1;
    while (a->ai_next != NULL) {
        a = a->ai_next;
    }

    a->ai_next = a2;
    return a1;
}

/*
 * Function: dup_val_addrinfo
 *
 * Purpose: Duplicates just the current val_addrinfo struct and its contents;
 *          does not duplicate the entire val_addrinfo linked list.
 *          Sets the ai_next pointer of the new val_addrinfo structure to NULL.
 *          The scope of this function is limited to this file.
 *
 * Parameters:
 *             a -- A pointer to a struct val_addrinfo variable which is to be
 *                  duplicated.
 *
 * Returns: A pointer to the duplicated struct val_addrinfo value.
 */
static struct val_addrinfo *
dup_val_addrinfo(const struct val_addrinfo *a)
{
    struct val_addrinfo *new_a = NULL;

    if (a == NULL)  
        return NULL;

    new_a = (struct val_addrinfo *) malloc(sizeof(struct val_addrinfo));
    if (new_a == NULL)  
        return NULL;

    bzero(new_a, sizeof(struct val_addrinfo));

    new_a->ai_flags = a->ai_flags;
    new_a->ai_family = a->ai_family;
    new_a->ai_socktype = a->ai_socktype;
    new_a->ai_protocol = a->ai_protocol;
    new_a->ai_addrlen = a->ai_addrlen;
    new_a->ai_addr = (struct sockaddr *) malloc(a->ai_addrlen);
    if (new_a->ai_addr == NULL) {
        free(new_a);
        return NULL;
    }

    memcpy(new_a->ai_addr, a->ai_addr, a->ai_addrlen);

    if (a->ai_canonname != NULL) {
        new_a->ai_canonname = strdup(a->ai_canonname);
        if (new_a->ai_canonname == NULL) {
            free(new_a->ai_addr);
            free(new_a);
            return NULL;
        }
    } else {
        new_a->ai_canonname = NULL;
    }
    new_a->ai_next = NULL;

    new_a->ai_val_status = a->ai_val_status;

    return new_a;
}


/*
 * Function: free_val_addrinfo
 *
 * Purpose: Free memory allocated for a val_addrinfo structure.  This
 *          function frees the entire linked list.  This function is
 *          used to free the value returned by val_getaddrinfo().
 *          This validator API function is global in scope; it can be
 *          called from anywhere in the program.
 *
 * Parameters:
 *          ainfo -- A pointer to the first element of a val_addrinfo
 *                   linked list.
 *
 * Returns:
 *          This function has no return value.
 *
 * See also: val_getaddrinfo()
 */
void
free_val_addrinfo(struct val_addrinfo *ainfo)
{
    struct val_addrinfo *acurr = ainfo;

    while (acurr != NULL) {
        struct val_addrinfo *anext = acurr->ai_next;
        if (acurr->ai_addr) {
            free(acurr->ai_addr);
        }
        if (acurr->ai_canonname) {
            free(acurr->ai_canonname);
        }
        free(acurr);
        acurr = anext;
    }
}

/*
 * Function: process_service_and_hints
 *
 * Purpose: Add additional val_addrinfo structures to the list depending on
 *          the service name and hints.
 *          The scope of this function is limited to this file.
 *
 * Parameters:
 *          val_status -- The validation status
 *            servname -- Name of the service.  Can be NULL.
 *               hints -- Hints to influence the result.  Can be NULL.
 *                 res -- Points to a linked list of val_addrinfo structures.
 *                        On return, this linked list may be augmented by
 *                        additional val_addrinfo structures depending on
 *                        the service name and hints.
 *
 * Returns: 0 if successful, a non-zero value if failure.
 */
static int
process_service_and_hints(val_status_t           val_status,
                          const char            *servname,
                          const struct addrinfo *hints,
                          struct val_addrinfo   **res)
{
    struct val_addrinfo *a1 = NULL;
    struct val_addrinfo *a2 = NULL;
    int proto_found         = 0;
    int created_locally     = 0;

    if (res == NULL)  
        return EAI_SERVICE;
   
    if (*res == NULL) {
        created_locally = 1;
        a1 = (struct val_addrinfo *) malloc(sizeof(struct val_addrinfo));
        if (a1 == NULL)  
            return EAI_MEMORY;

        bzero(a1, sizeof(struct val_addrinfo));
        *res = a1;
    } else {
        a1 = *res;
    }

    if (!a1)
        return 0;

    a1->ai_val_status = val_status;

    /*
     * Flags 
     */
    if ((hints != NULL) && (hints->ai_flags != 0)) {
        a1->ai_flags = hints->ai_flags;
    } else {
#if defined(AI_V4MAPPED) && defined(AI_ADDRCONFIG)
        a1->ai_flags = (AI_V4MAPPED | AI_ADDRCONFIG);
#else
        a1->ai_flags = 0;       /* ?? something else? */
#endif
    }

    /*
     * Check if we have to return val_addrinfo structures for the
       SOCK_STREAM socktype
     */
    if ((hints == NULL || hints->ai_socktype == 0 
         || hints->ai_socktype == SOCK_STREAM) 
        && (servname == NULL 
            || getservbyname(servname, "tcp") != NULL
            || ( atoi(servname) && getservbyport(atoi(servname), "tcp") )
           ) 
       )  {
        a1->ai_socktype = SOCK_STREAM;
        a1->ai_protocol = IPPROTO_TCP;
        a1->ai_next = NULL;
        proto_found = 1;
    }

    /*
     * Check if we have to return val_addrinfo structures for the
       SOCK_DGRAM socktype
     */
    if ((hints == NULL || hints->ai_socktype == 0
         || hints->ai_socktype == SOCK_DGRAM) 
        && (servname == NULL 
            || getservbyname(servname, "udp") != NULL
            || ( atoi(servname) && getservbyport(atoi(servname), "udp") )
           )
       )  {
        if (proto_found) {
            a2 = dup_val_addrinfo(a1);
            if (a2 == NULL) 
              return EAI_MEMORY;
            a1->ai_next = a2;
            a1 = a2;
        }
        a1->ai_socktype = SOCK_DGRAM;
        a1->ai_protocol = IPPROTO_UDP;
        proto_found = 1;
    }

    /*
     * Check if we have to return val_addrinfo structures for the
       SOCK_RAW socktype
     */
    if ((hints == NULL || hints->ai_socktype == 0
         || hints->ai_socktype == SOCK_RAW) 
        && (servname == NULL 
            || getservbyname(servname, "ip") != NULL
            || ( atoi(servname) && getservbyport(atoi(servname), "ip") )
           )
       )  {
        if (proto_found) {
            a2 = dup_val_addrinfo(a1);
            // xxx-note: uggh. may have augmented caller's res ptr
            //     above, so I hate returning an error. But then
            //     returning 0 in the face of an error doesn't
            //     seem right either.
            if (a2 == NULL)
                return EAI_MEMORY;
            a1->ai_next = a2;
            a1 = a2;
        }
        a1->ai_socktype = SOCK_RAW;
        a1->ai_protocol = IPPROTO_IP;
        proto_found = 1;
    }

    if (proto_found) {
        return 0;
    } else {
        /*
         * no valid protocol found 
         */
        // xxx-audit: function documentation doesn't mention anything
        //     about possibly nuking the caller's ptr and freeing their
        //     memory. Maybe there needs to be a better separation
        //     between memory allocated here and caller's memory.
        /* if top memory allocated locally, delete */
        if (created_locally) {
            *res = NULL;
            free_val_addrinfo(a1);
        }
        return EAI_SERVICE;
    }
}                               /* end process_service_and_hints */


/*
 * Function: get_addrinfo_from_etc_hosts
 *
 * Purpose: Read the ETC_HOSTS file and check if it contains the given name.
 *          The scope of this function is limited to this file.
 *
 * Parameters:
 *              ctx -- The validation context.  Must not be NULL.
 *         nodename -- Name of the node.  Must not be NULL.
 *         servname -- Name of the service.  Can be NULL.
 *            hints -- Hints that influence the results.  Can be NULL.
 *              res -- Pointer to a variable of type val_addrinfo *.  On
 *                     successful return, this will contain a linked list
 *                     of val_addrinfo structures.
 *
 * Returns: 0 if successful, and a non-zero value on error.
 *
 * See also: get_addrinfo_from_dns(), val_getaddrinfo()
 */
static int
get_addrinfo_from_etc_hosts(const val_context_t *ctx,
                            const char *nodename,
                            const char *servname,
                            const struct addrinfo *hints,
                            struct val_addrinfo **res)
{
    struct hosts   *hs = NULL;
    struct val_addrinfo *retval = NULL;

    if (res == NULL)
        return 0;

    val_log(ctx, LOG_DEBUG, "Parsing /etc/hosts");

    /*
     * Parse the /etc/hosts/ file 
     */
    hs = parse_etc_hosts(nodename);

    while (hs) {
        int             alias_index = 0;
        struct in_addr  ip4_addr;
        struct in6_addr ip6_addr;
        struct hosts   *h_prev = hs;
        struct val_addrinfo *ainfo;

        ainfo =
            (struct val_addrinfo *) malloc(sizeof(struct val_addrinfo));
        if (!ainfo) {
            if (retval)
                free_val_addrinfo(retval);
            return EAI_MEMORY;
        }

        val_log(ctx, LOG_DEBUG, "{");
        val_log(ctx, LOG_DEBUG, "  Address: %s", hs->address);
        val_log(ctx, LOG_DEBUG, "  Canonical Hostname: %s",
                hs->canonical_hostname);
        val_log(ctx, LOG_DEBUG, "  Aliases:");

        while (hs->aliases[alias_index] != NULL) {
            val_log(ctx, LOG_DEBUG, "   %s", hs->aliases[alias_index]);
            alias_index++;
        }

        val_log(ctx, LOG_DEBUG, "}");

        bzero(ainfo, sizeof(struct val_addrinfo));
        bzero(&ip4_addr, sizeof(struct in_addr));
        bzero(&ip6_addr, sizeof(struct in6_addr));

        /*
         * Check if the address is an IPv4 address 
         */
        if (inet_pton(AF_INET, hs->address, &ip4_addr) > 0) {
            struct sockaddr_in *saddr4 =
                (struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));
            if (saddr4 == NULL) {
                if (retval)
                    free_val_addrinfo(retval);
                free_val_addrinfo(ainfo);
                return EAI_MEMORY;
            }
            bzero(saddr4, sizeof(struct sockaddr_in));
            ainfo->ai_family = AF_INET;
            saddr4->sin_family = AF_INET;
            ainfo->ai_addrlen = sizeof(struct sockaddr_in);
            memcpy(&(saddr4->sin_addr), &ip4_addr, sizeof(struct in_addr));
            ainfo->ai_addr = (struct sockaddr *) saddr4;
            ainfo->ai_canonname = NULL;
        }
        /*
         * Check if the address is an IPv6 address 
         */
        else if (inet_pton(AF_INET6, hs->address, &ip6_addr) > 0) {
            struct sockaddr_in6 *saddr6 = (struct sockaddr_in6 *)
                malloc(sizeof(struct sockaddr_in6));
            if (saddr6 == NULL) {
                if (retval)
                    free_val_addrinfo(retval);
                free_val_addrinfo(ainfo);
                return EAI_MEMORY;
            }
            bzero(saddr6, sizeof(struct sockaddr_in6));
            ainfo->ai_family = AF_INET6;
            saddr6->sin6_family = AF_INET6;
            ainfo->ai_addrlen = sizeof(struct sockaddr_in6);
            memcpy(&(saddr6->sin6_addr), &ip6_addr,
                   sizeof(struct in6_addr));
            ainfo->ai_addr = (struct sockaddr *) saddr6;
            ainfo->ai_canonname = NULL;
        } else {
            free_val_addrinfo(ainfo);
            continue;
        }

        ainfo->ai_val_status = VAL_LOCAL_ANSWER;

        /*
         * Expand the results based on servname and hints 
         */
        if (process_service_and_hints
            (ainfo->ai_val_status, servname, hints, &ainfo) != 0) {
            free_val_addrinfo(ainfo);
            if (retval)
                free_val_addrinfo(retval);
            return EAI_SERVICE;
        }

        if (retval) {
            retval = append_val_addrinfo(retval, ainfo);
        } else {
            retval = ainfo;
        }

        hs = hs->next;
        FREE_HOSTS(h_prev);
    }
    val_log(ctx, LOG_DEBUG, "Parsing /etc/hosts OK");

    *res = retval;
    if (retval) {
        return 0;
    } else {
        return EAI_NONAME;
    }
}                               /* get_addrinfo_from_etc_hosts() */


/*
 * Function: get_addrinfo_from_result
 *
 * Purpose: Converts the result value from the validator (which is
 *          in the form of a linked list of val_result_chain structures)
 *          into a liked list of val_addrinfo structures.
 *          The scope of this function is limited to this file.
 *
 * Parameters:
 *              ctx -- The validation context.
 *          results -- The results obtained from the val_resolve_and_check
 *                     method in the validator API.
 *         servname -- The service name.  Can be NULL.
 *            hints -- Hints that influence the returned results.  Can be NULL.
 *              res -- A pointer to a variable of type struct val_addrinfo *.
 *                     On successful return, this will contain a linked list
 *                     of val_addrinfo structures.
 *
 * Returns: 0 on success, and a non-zero error-code on error.
 *
 * See also: get_addrinfo_from_etc_hosts(), val_addrinfo()
 */
static int
get_addrinfo_from_result(const val_context_t *ctx,
                         struct val_result_chain *results,
                         int val_status,
                         const char *servname,
                         const struct addrinfo *hints,
                         struct val_addrinfo **res)
{
    struct val_addrinfo *ainfo_head = NULL;
    struct val_addrinfo *ainfo_tail = NULL;
    struct val_result_chain *result = NULL;
    char *canonname                 = NULL;

    if (res == NULL)
        return 0;

    val_log(ctx, LOG_DEBUG,
            "get_addrinfo_from_result called with val_status = %d [%s]",
            val_status, p_val_error(val_status));

    if (!results) {
        val_log(ctx, LOG_DEBUG, "rrset is null");
    }

    /*
     * Loop for each result in the linked list of val_result_chain structures 
     */
    for (result = results; result != NULL; result = result->val_rc_next) {
        struct val_rrset *rrset = result->val_rc_trust ?
            result->val_rc_trust->val_ac_rrset : NULL;

        if (rrset != NULL) {
            struct rr_rec  *rr = rrset->val_rrset_data;

            /*
             * Check if the AI_CANONNAME flag is specified 
             */
            if (hints && (hints->ai_flags & AI_CANONNAME)
                && (canonname == NULL)) {
                char            dname[NS_MAXDNAME];
                bzero(dname, sizeof(dname));
                if (ns_name_ntop
                    (rrset->val_rrset_name_n, dname, sizeof(dname)) < 0) {
                    /*
                     * error 
                     */
                    val_log(ctx, LOG_DEBUG, "error in ns_name_ntop");
                } else {
                    val_log(ctx, LOG_DEBUG, "duplicating the canonname");
                    canonname =
                        (char *) malloc((strlen(dname) + 1) *
                                        sizeof(char));
                    if (canonname != NULL)
                        memcpy(canonname, dname, strlen(dname) + 1);
                }
            }

            /*
             * Loop for each rr in the linked list of rr_rec structures 
             */
            while (rr != NULL) {
                struct val_addrinfo *ainfo = NULL;

                ainfo = (struct val_addrinfo *)
                    malloc(sizeof(struct val_addrinfo));
                if (ainfo == NULL) {
                    if (canonname)
                        FREE(canonname);
                    return EAI_MEMORY;
                }
                bzero(ainfo, sizeof(struct val_addrinfo));

                /*
                 * Check if the record-type is A 
                 */
                if (rrset->val_rrset_type_h == ns_t_a) {
                    struct sockaddr_in *saddr4 = (struct sockaddr_in *)
                        malloc(sizeof(struct sockaddr_in));
                    if (saddr4 == NULL) {
                        if (ainfo_head)
                            free_val_addrinfo(ainfo_head);
                        free_val_addrinfo(ainfo);
                        if (canonname)
                            FREE(canonname);
                        return EAI_MEMORY;
                    }
                    val_log(ctx, LOG_DEBUG, "rrset of type A found");
                    saddr4->sin_family = AF_INET;
                    ainfo->ai_family = AF_INET;
                    ainfo->ai_addrlen = sizeof(struct sockaddr_in);
                    memcpy(&(saddr4->sin_addr.s_addr), rr->rr_rdata,
                           rr->rr_rdata_length_h);
                    ainfo->ai_addr = (struct sockaddr *) saddr4;
                }
                /*
                 * Check if the record-type is AAAA 
                 */
                else if (rrset->val_rrset_type_h == ns_t_aaaa) {
                    struct sockaddr_in6 *saddr6 = (struct sockaddr_in6 *)
                        malloc(sizeof(struct sockaddr_in6));
                    if (saddr6 == NULL) {
                        if (ainfo_head)
                            free_val_addrinfo(ainfo_head);
                        free_val_addrinfo(ainfo);
                        if (canonname)
                            FREE(canonname);
                        return EAI_MEMORY;
                    }
                    val_log(ctx, LOG_DEBUG, "rrset of type AAAA found");
                    saddr6->sin6_family = AF_INET6;
                    ainfo->ai_family = AF_INET6;
                    ainfo->ai_addrlen = sizeof(struct sockaddr_in6);
                    memcpy(&(saddr6->sin6_addr.s6_addr), rr->rr_rdata,
                           rr->rr_rdata_length_h);
                    ainfo->ai_addr = (struct sockaddr *) saddr6;
                } else {
                    free_val_addrinfo(ainfo);
                    rr = rr->rr_next;
                    continue;
                }

                if (canonname)
                    ainfo->ai_canonname = strdup(canonname);
                ainfo->ai_val_status = val_status;

                /*
                 * Expand the results based on servname and hints 
                 */
                if (process_service_and_hints(val_status, servname, 
                                              hints, &ainfo) 
                    == EAI_SERVICE) {
                    free_val_addrinfo(ainfo_head);
                    free_val_addrinfo(ainfo);
                    return EAI_SERVICE;
                }

                if (ainfo_head == NULL) {
                    ainfo_head = ainfo;
                } else {
                    ainfo_tail->ai_next = ainfo;
                }

                if (ainfo)
                    ainfo_tail = ainfo;

                rr = rr->rr_next;
            }
        }
    }

    *res = ainfo_head;
    if (ainfo_head) {
        return 0;
    } else {
        if (canonname)
            free(canonname);
        return EAI_NONAME;
    }
}                               /* get_addrinfo_from_result() */

/*
 * Function: get_addrinfo_from_dns
 *
 * Purpose: Resolve the nodename from DNS and fill in the val_addrinfo
 *          return value.  The scope of this function is limited to this
 *          file, and is called from val_addrinfo().
 *
 * Parameters:
 *              ctx -- The validation context.
 *         nodename -- The name of the node.  This value must not be NULL.
 *         servname -- The service name.  Can be NULL.
 *            hints -- Hints to influence the return value.  Can be NULL.
 *              res -- A pointer to a variable of type (struct val_addrinfo *) to
 *                     hold the result.  The caller must free this return value
 *                     using free_val_addrinfo().
 *
 * Returns: 0 on success and a non-zero value on error.
 *
 * See also: val_getaddrinfo()
 */
static int
get_addrinfo_from_dns(val_context_t *ctx,
                      const char *nodename,
                      const char *servname,
                      const struct addrinfo *hints,
                      struct val_addrinfo **res)
{
    struct val_result_chain *results = NULL;
    struct val_addrinfo *ainfo       = NULL;
    u_char name_n[NS_MAXCDNAME];
    int retval                       = 0;
    int ret                          = 0;

    if (res == NULL)
        return 0;

    val_log(ctx, LOG_DEBUG, "get_addrinfo_from_dns() called");

    /*
     * Check if we need to return IPv4 addresses based on the hints 
     */
    if (hints == NULL || hints->ai_family == AF_UNSPEC
        || hints->ai_family == AF_INET) {

        val_log(ctx, LOG_DEBUG, "checking for A records");

        /*
         * Query the validator 
         */
        if ((retval =
             ns_name_pton(nodename, name_n, sizeof(name_n))) != -1) {
            if ((retval =
                 val_resolve_and_check(ctx, name_n, ns_c_in, ns_t_a, 0,
                                       &results)) != VAL_NO_ERROR) {
                val_log(ctx, LOG_DEBUG, "val_resolve_and_check failed");
            }
        } else {
            val_log(ctx, LOG_DEBUG, "ns_name_pton failed");
        }

        /*
         * Convert the validator result into val_addrinfo 
         */
        if (results && results->val_rc_trust && retval == VAL_NO_ERROR) {
            struct val_addrinfo *ainfo_new = NULL;
            ret =
                get_addrinfo_from_result(ctx, results,
                                         results->val_rc_status, servname,
                                         hints, &ainfo_new);
            if (ainfo_new) {
                val_log(ctx, LOG_DEBUG, "A records found");
                ainfo = append_val_addrinfo(ainfo, ainfo_new);
            } else {
                val_log(ctx, LOG_DEBUG, "A records not found");
            }
        }
        val_free_result_chain(results);
        results = NULL;
        if (ret == EAI_SERVICE) {
            if (ainfo)
                free_val_addrinfo(ainfo);

            return EAI_SERVICE;
        }
    }

    /*
     * Check if we need to return IPv6 addresses based on the hints 
     */
    if (hints == NULL || hints->ai_family == AF_UNSPEC
        || hints->ai_family == AF_INET6) {

        val_log(ctx, LOG_DEBUG, "checking for AAAA records");

        /*
         * Query the validator 
         */
        if ((retval =
             ns_name_pton(nodename, name_n, sizeof(name_n))) != -1) {
            if ((retval =
                 val_resolve_and_check((val_context_t *) ctx, name_n,
                                       ns_c_in, ns_t_aaaa, 0,
                                       &results)) != VAL_NO_ERROR) {
                val_log(ctx, LOG_DEBUG, "val_resolve_and_check failed");
            }
        } else {
            val_log(ctx, LOG_DEBUG, "ns_name_pton failed");
        }

        /*
         * Convert the validator result into val_addrinfo 
         */
        if (results && results->val_rc_trust && retval == VAL_NO_ERROR) {
            struct val_addrinfo *ainfo_new = NULL;
            ret =
                get_addrinfo_from_result(ctx, results,
                                         results->val_rc_status, servname,
                                         hints, &ainfo_new);
            if (ainfo_new) {
                val_log(ctx, LOG_DEBUG, "AAAA records found");
                ainfo = append_val_addrinfo(ainfo, ainfo_new);
            } else {
                val_log(ctx, LOG_DEBUG, "AAAA records not found");
            }
        }
        val_free_result_chain(results);
        results = NULL;
        if (ret == EAI_SERVICE) {
            if (ainfo)
                free_val_addrinfo(ainfo);

            return EAI_SERVICE;
        }
    }

    if (ainfo) {
        *res = ainfo;
        return 0;
    } else {
        return EAI_NONAME;
    }

}                               /* get_addrinfo_from_dns() */


/*
 * Function: val_getaddrinfo
 *
 * Purpose: A DNSSEC-aware version of the getaddrinfo() function.
 *
 * Parameters:
 *              ctx -- The validator context. Can be NULL for default value.
 *         nodename -- The name of the node or its IP address.  Can be NULL, or
 *                     a domain name, or an IPv4 or IPv6 address string.
 *         servname -- The name of the service.  Can be NULL.
 *            hints -- Hints to influence the result value.  Can be NULL.
 *              res -- A pointer to a variable of type (struct val_addrinfo*) to
 *                     hold the result.  The caller must free this return value
 *                     using free_val_addrinfo().
 *
 *         Note that at least one of nodename or servname must be a non-NULL value.
 *
 * Returns: 0 if successful, a non-zero error code on error.
 *
 * See also: getaddrinfo(3), free_val_addrinfo()
 */
int
val_getaddrinfo(val_context_t * ctx,
                const char *nodename, const char *servname,
                const struct addrinfo *hints, struct val_addrinfo **res)
{
    struct in_addr  ip4_addr;
    struct in6_addr ip6_addr;
    struct val_addrinfo *ainfo4 = NULL;
    struct val_addrinfo *ainfo6 = NULL;
    int             is_ip4 = 0;
    int             is_ip6 = 0;
    int             retval = 0;
    const char     *localhost4 = "127.0.0.1";
    const char     *localhost6 = "::1";
    const char     *nname = nodename;
    val_context_t  *context = NULL;

    if (res == NULL)
        return 0;

    *res = NULL;

    if (ctx == NULL) {
        if (VAL_NO_ERROR != (retval = val_create_context(NULL, &context)))
            return EAI_FAIL;
    } else
        context = (val_context_t *) ctx;

    val_log(context, LOG_DEBUG,
            "val_getaddrinfo called with nodename = %s, servname = %s",
            nodename == NULL ? "(null)" : nodename,
            servname == NULL ? "(null)" : servname);

    /*
     * Check if hints is non-NULL and that at least one of nodename
       or servname is non-NULL
     */
    if ( (NULL == nodename && NULL == servname) ||
         (NULL == hints) )  {
        retval = EAI_NONAME;
        goto done;
    }

    bzero(&ip4_addr, sizeof(struct in_addr));
    bzero(&ip6_addr, sizeof(struct in6_addr));

    /*
     * if nodename is blank or hints includes ipv4 or unspecified,
     * use IPv4 localhost 
     */
    if (NULL == nodename &&
        ( AF_INET   == hints->ai_family || 
          AF_UNSPEC == hints->ai_family )
        ) {
        nname = localhost4;
    }

    /*
     * check for IPv4 addresses 
     */
    if (NULL != nname && inet_pton(AF_INET, nname, &ip4_addr) > 0) {

        struct sockaddr_in *saddr4 =
            (struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));
        if (saddr4 == NULL) {
            retval = EAI_MEMORY;
            goto done;
        }
        ainfo4 =
            (struct val_addrinfo *) malloc(sizeof(struct val_addrinfo));
        if (ainfo4 == NULL) {
            free(saddr4);
            retval = EAI_MEMORY;
            goto done;
        }

        is_ip4 = 1;

        bzero(ainfo4, sizeof(struct val_addrinfo));
        bzero(saddr4, sizeof(struct sockaddr_in));

        saddr4->sin_family    = AF_INET;
        ainfo4->ai_family     = AF_INET;
        memcpy(&(saddr4->sin_addr), &ip4_addr, sizeof(struct in_addr));
        ainfo4->ai_addr       = (struct sockaddr *) saddr4;
        saddr4                = NULL;
        ainfo4->ai_addrlen    = sizeof(struct sockaddr_in);
        ainfo4->ai_canonname  = NULL;
        ainfo4->ai_val_status = VAL_LOCAL_ANSWER;

        if (process_service_and_hints(ainfo4->ai_val_status, servname, 
                                      hints, &ainfo4)
            == EAI_SERVICE) {
            free_val_addrinfo(ainfo4);
            retval = EAI_SERVICE;
            goto done;
        }

        *res = ainfo4;
        retval = 0;
        goto done;
    }

    /*
     * if nodename is blank and hints includes ipv6 or unspecified,
     * use IPv6 localhost 
     */
    if (NULL == nodename &&
        ( AF_INET6  == hints->ai_family || 
          AF_UNSPEC == hints->ai_family )
        ) {
        nname = localhost6;
    }

    /*
     * Check for IPv6 address 
     */
    if (nname != NULL && inet_pton(AF_INET6, nname, &ip6_addr) > 0) {

        struct sockaddr_in6 *saddr6 =
            (struct sockaddr_in6 *) malloc(sizeof(struct sockaddr_in6));
        if (saddr6 == NULL) {
            retval = EAI_MEMORY;
            goto done;
        }
        ainfo6 =
            (struct val_addrinfo *) malloc(sizeof(struct val_addrinfo));
        if (ainfo6 == NULL) {
            free(saddr6);
            retval = EAI_MEMORY;
            goto done;
        }

        is_ip6 = 1;

        bzero(ainfo6, sizeof(struct val_addrinfo));
        bzero(saddr6, sizeof(struct sockaddr_in6));

        saddr6->sin6_family   = AF_INET6;
        ainfo6->ai_family     = AF_INET6;
        memcpy(&(saddr6->sin6_addr), &ip6_addr, sizeof(struct in6_addr));
        ainfo6->ai_addr       = (struct sockaddr *) saddr6;
        saddr6                = NULL;
        ainfo6->ai_addrlen    = sizeof(struct sockaddr_in6);
        ainfo6->ai_canonname  = NULL;
        ainfo6->ai_val_status = VAL_LOCAL_ANSWER;

        if (process_service_and_hints(ainfo6->ai_val_status, servname, 
                                      hints, &ainfo6) 
            == EAI_SERVICE) {
            free_val_addrinfo(ainfo6);
            retval = EAI_SERVICE;
            goto done;
        }

        if (NULL != *res) {
            *res = append_val_addrinfo(*res, ainfo6);
        } else {
            *res = ainfo6;
        }

        retval = 0;
        goto done;
    }

    /*
     * If nodename was specified and was not an IPv4 or IPv6
     * address, get its information from local store or from dns
     */
    if (nodename && !is_ip4 && !is_ip6) {
        /*
         * First check ETC_HOSTS file
         * * XXX: TODO check the order in the ETC_HOST_CONF file
         */
        if (get_addrinfo_from_etc_hosts(context, nodename, servname, 
                                        hints, res) 
            == EAI_SERVICE) {
            retval = EAI_SERVICE;
        } else if (*res != NULL) {
            retval = 0;
        }

        /*
         * Try DNS
         */
        else if (get_addrinfo_from_dns(context, nodename, servname, 
                                       hints, res) 
                 == EAI_SERVICE) {
            retval = EAI_SERVICE;
        } else if (*res != NULL) {
            retval = 0;
        } else {
            retval = EAI_NONAME;
        }
    }

  done:
    if ( (ctx == NULL) && context )
        val_free_context(context);
    return retval;

}                               /* val_getaddrinfo() */
