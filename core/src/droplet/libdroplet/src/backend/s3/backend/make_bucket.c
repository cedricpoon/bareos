/*
 * Copyright (C) 2020-2021 Bareos GmbH & Co. KG
 * Copyright (C) 2010 SCALITY SA. All rights reserved.
 * http://www.scality.com
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SCALITY SA ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SCALITY SA OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of SCALITY SA.
 *
 * https://github.com/scality/Droplet
 */

#include "dropletp.h"
#include "droplet/s3/s3.h"

/**
 * make a bucket
 *
 * @param ctx
 * @param bucket
 * @param location_constraint
 * @param sysmd
 *
 * @return
 */
dpl_status_t dpl_s3_make_bucket(dpl_ctx_t* ctx,
                                const char* bucket,
                                const dpl_sysmd_t* sysmd,
                                char** locationp)
{
  int ret, ret2;
  dpl_conn_t* conn = NULL;
  char header[dpl_header_size];
  u_int header_len;
  struct iovec iov[10];
  int n_iov = 0;
  int connection_close = 0;
  char* location_constraint_str;
  char data_str[1024];
  u_int data_len;
  dpl_dict_t* headers_request = NULL;
  dpl_dict_t* headers_reply = NULL;
  dpl_req_t* req = NULL;
  dpl_s3_req_mask_t req_mask = 0u;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  req = dpl_req_new(ctx);
  if (NULL == req) {
    ret = DPL_ENOMEM;
    goto end;
  }

  dpl_req_set_method(req, DPL_METHOD_PUT);

  if (NULL == bucket) {
    ret = DPL_EINVAL;
    goto end;
  }

  ret2 = dpl_req_set_bucket(req, bucket);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret2 = dpl_req_set_resource(req, "/");
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  if (sysmd) {
    if (sysmd->mask & DPL_SYSMD_MASK_LOCATION_CONSTRAINT) {
      if (DPL_LOCATION_CONSTRAINT_US_STANDARD == sysmd->location_constraint) {
        data_str[0] = 0;
        data_len = 0;
      } else {
        location_constraint_str
            = dpl_location_constraint_str(sysmd->location_constraint);
        if (NULL == location_constraint_str) {
          ret = DPL_ENOMEM;
          goto end;
        }

        snprintf(data_str, sizeof(data_str),
                 "<CreateBucketConfiguration "
                 "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">\n"
                 "<LocationConstraint>%s</LocationConstraint>\n"
                 "</CreateBucketConfiguration>\n",
                 location_constraint_str);

        data_len = strlen(data_str);
      }
    } else {
      data_str[0] = 0;
      data_len = 0;
    }
  } else {
    data_str[0] = 0;
    data_len = 0;
  }

  dpl_req_set_data(req, data_str, data_len);

  if (sysmd) {
    if (sysmd->mask & DPL_SYSMD_MASK_CANNED_ACL)
      dpl_req_set_canned_acl(req, sysmd->canned_acl);

    if (sysmd->mask & DPL_SYSMD_MASK_STORAGE_CLASS)
      dpl_req_set_storage_class(req, sysmd->storage_class);
  }

  // build request
  ret2 = dpl_s3_req_build(req, req_mask, &headers_request);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret2 = dpl_try_connect(ctx, req, &conn);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret2 = dpl_add_host_to_headers(req, headers_request);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret2 = dpl_s3_add_authorization_to_headers(req, headers_request, NULL, NULL);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header,
                                  sizeof(header), &header_len);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  iov[n_iov].iov_base = header;
  iov[n_iov].iov_len = header_len;
  n_iov++;

  // final crlf
  iov[n_iov].iov_base = "\r\n";
  iov[n_iov].iov_len = 2;
  n_iov++;

  // buffer
  iov[n_iov].iov_base = data_str;
  iov[n_iov].iov_len = data_len;
  n_iov++;

  ret2 = dpl_conn_writev_all(conn, iov, n_iov, conn->ctx->write_timeout);
  if (DPL_SUCCESS != ret2) {
    DPL_TRACE(conn->ctx, DPL_TRACE_ERR, "writev failed");
    connection_close = 1;
    ret = ret2;
    goto end;
  }

  ret2 = dpl_read_http_reply(conn, 1, NULL, NULL, &headers_reply,
                             &connection_close);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret = DPL_SUCCESS;

end:

  if (NULL != conn) {
    if (1 == connection_close)
      dpl_conn_terminate(conn);
    else
      dpl_conn_release(conn);
  }

  if (NULL != headers_reply) dpl_dict_free(headers_reply);

  if (NULL != headers_request) dpl_dict_free(headers_request);

  if (NULL != req) dpl_req_free(req);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}
