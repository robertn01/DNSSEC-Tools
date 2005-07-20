
/*
 * Portions Copyright (c) 1995, 1996, 1997 by Trusted Information Systems, Inc.
 *
 * Permission to use, copy modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND TRUSTED INFORMATION SYSTEMS
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
 * TRUSTED INFORMATION SYSTEMS BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THE SOFTWARE.
 */
/*
 * Copyright 2005 SPARTA, Inc.  All rights reserved.
 * See the COPYING file distributed with this software for details.
 */ 
#ifndef VAL_SUPPORT_H
#define VAL_SUPPORT_H

void free_name_server (struct name_server **ns);
void free_name_servers (struct name_server **ns);
void res_sq_free_rr_recs (struct rr_rec **rr);
void res_sq_free_rrset_recs (struct rrset_rec **set);
int add_to_qname_chain (  struct qname_chain  **qnames,
                                const u_int8_t      *name_n);
int qname_chain_first_name (struct qname_chain *qnames, const u_int8_t *name_n);
void free_qname_chain (struct qname_chain **qnames);
void free_domain_info_ptrs (struct domain_info *di);
int is_tail (u_int8_t *full, u_int8_t *tail);
int nxt_sig_match (u_int8_t *owner, u_int8_t *next, u_int8_t *signer);
int add_to_set (struct rrset_rec *rr_set,u_int16_t rdata_len_h,u_int8_t *rdata);
int add_as_sig (struct rrset_rec *rr_set,u_int16_t rdata_len_h,u_int8_t *rdata);
int init_rr_set (   struct rrset_rec    *new_set,
                    u_int8_t            *name_n,
                    u_int16_t           type_h,
                    u_int16_t           set_type_h,
                    u_int16_t           class_h,
                    u_int32_t           ttl_h,
                    u_int8_t            *rdata_n,
                    int                 from_section,
                    int                 authoritive_answer,
                    int                 tsig_ed);
struct rrset_rec *find_rr_set (
                                struct rrset_rec    **the_list,
                                u_int8_t            *name_n,
                                u_int16_t           type_h,
                                u_int16_t           set_type_h,
                                u_int16_t           class_h,
                                u_int32_t           ttl_h,
                                u_int8_t            *rdata_n,
                                int                 from_section,
                                int                 authoritive_answer,
                                int                 tsig_ed);
int check_label_count (
                            struct rrset_rec    *the_set,
                            struct rr_rec       *the_sig,
                            int                 *is_a_wildcard);
int prepare_empty_nxdomain (struct rrset_rec    **answers,
                            const u_int8_t      *query_name_n,
                            u_int16_t           query_type_h,
                            u_int16_t           query_class_h);
int decompress( u_int8_t    **rdata,
                u_int8_t    *response,
                int         rdata_index,
                u_int8_t    *end,
                u_int16_t   type_h,
                u_int16_t   *rdata_len_h);

int extract_from_rr (   u_int8_t *response,
                        int *response_index,
                        u_int8_t *end,
                        u_int8_t *name_n,
                        u_int16_t *type_h,
                        u_int16_t *set_type_h,
                        u_int16_t *class_h,
                        u_int32_t *ttl_h,
                        u_int16_t *rdata_length_h,
                        int *rdata_index);

#endif /* VAL_SUPPORT_H */
