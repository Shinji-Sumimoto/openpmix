/*
 * Copyright (c) 2013-2015 Intel, Inc. All rights reserved
 * Copyright (c) 2015      Artem Y. Polyakov <artpol84@gmail.com>.
 *                         All rights reserved.
 * $COPYRIGHT$
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer listed
 *   in this license in the documentation and/or other materials
 *   provided with the distribution.
 *
 * - Neither the name of the copyright holders nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * The copyright holders provide no reassurances that the source code
 * provided does not infringe any patent, copyright, or any other
 * intellectual property rights of third parties.  The copyright holders
 * disclaim any liability to any recipient for claims brought against
 * recipient by any third party for infringement of that parties
 * intellectual property rights.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * $HEADER$
 *
 * PMIx provides a "function-shipping" approach to support for
 * implementing the server-side of the protocol. This method allows
 * resource managers to implement the server without being burdened
 * with PMIx internal details. Accordingly, each PMIx API is mirrored
 * here in a function call to be provided by the server. When a
 * request is received from the client, the corresponding server function
 * will be called with the information.
 *
 * Any functions not supported by the RM can be indicated by a NULL for
 * the function pointer. Client calls to such functions will have a
 * "not supported" error returned.
 */

#ifndef PMIx_SERVER_API_H
#define PMIx_SERVER_API_H

#include <pmix/autogen/config.h>

#include <stdint.h>
#include <string.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h> /* for struct timeval */
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif
#ifdef HAVE_NET_UIO_H
#include <net/uio.h>
#endif


/* Symbol transforms */
#include <pmix/rename.h>

/* Structure and constant definitions */
#include <pmix/pmix_common.h>

BEGIN_C_DECLS

/****    SERVER FUNCTION-SHIPPED APIs    ****/
/* NOTE: for performance purposes, the host server is required to
 * return as quickly as possible from all functions. Execution of
 * the function is thus to be done asynchronously so as to allow
 * the PMIx server support library to handle multiple client requests
 * as quickly and scalably as possible.
 *
 * ALL data passed to the host server functions is "owned" by the
 * PMIX server support library and MUST NOT be free'd. Data returned
 * by the host server via callback function is owned by the host
 * server, which is free to release it upon return from the callback */


/* Notify the host server that a client called PMIx_Finalize- note
 * that the client will be in a blocked state until the host server
 * executes the callback function, thus allowing the PMIx server support
 * library to release the client */
typedef pmix_status_t (*pmix_server_finalized_fn_t)(const char nspace[], int rank, void* server_object,
                                                    pmix_op_cbfunc_t cbfunc, void *cbdata);

/* A local client called PMIx_Abort - note that the client will be in a blocked
 * state until the host server executes the callback function, thus
 * allowing the PMIx server support library to release the client. The
 * array of procs indicates which processes are to be terminated. A NULL
 * indicates that all procs in the client's nspace are to be terminated */
typedef pmix_status_t (*pmix_server_abort_fn_t)(const char nspace[], int rank, void *server_object,
                                                int status, const char msg[],
                                                pmix_proc_t procs[], size_t nprocs,
                                                pmix_op_cbfunc_t cbfunc, void *cbdata);

/* At least one client called either PMIx_Fence or PMIx_Fence_nb. In either case,
 * the host server will be called via a non-blocking function to execute
 * the specified operation once all participating local procs have
 * contributed. All processes in the specified array are required to participate
 * in the Fence[_nb] operation. The callback is to be executed once each daemon
 * hosting at least one participant has called the host server's fencenb function.
 *
 * The provided data is to be collectively shared with all PMIx
 * servers involved in the fence operation, and returned in the modex
 * cbfunc. A _NULL_ data value indicates that the local procs had
 * no data to contribute */
typedef pmix_status_t (*pmix_server_fencenb_fn_t)(const pmix_proc_t procs[], size_t nprocs,
                                                  char *data, size_t ndata,
                                                  pmix_modex_cbfunc_t cbfunc, void *cbdata);


/* Used by the PMIx server to request its local host contact the
 * PMIx server on the remote node that hosts the specified proc to
 * obtain and return a direct modex blob for that proc */
typedef pmix_status_t (*pmix_server_dmodex_req_fn_t)(const char nspace[], int rank,
                                                     pmix_modex_cbfunc_t cbfunc, void *cbdata);


/* Publish data per the PMIx API specification. The callback is to be executed
 * upon completion of the operation. The host server is not required to guarantee
 * support for the requested scope - i.e., the server does not need to return an
 * error if the data store doesn't support scope-based isolation. However, the
 * server must return an error (a) if the key is duplicative within the storage
 * scope, and (b) if the server does not allow overwriting of published info by
 * the original publisher - it is left to the discretion of the host server to
 * allow info-key-based flags to modify this behavior. The persist flag indicates
 * how long the server should retain the data. The nspace/rank of the publishing
 * process is also provided and is expected to be returned on any subsequent
 * lookup request */
typedef pmix_status_t (*pmix_server_publish_fn_t)(const char nspace[], int rank,
                                                  pmix_scope_t scope, pmix_persistence_t persist,
                                                  const pmix_info_t info[], size_t ninfo,
                                                  pmix_op_cbfunc_t cbfunc, void *cbdata);

/* Lookup published data. The host server will be passed a NULL-terminated array
 * of string keys along with the scope within which the data is expected to have
 * been published. The host server is not required to guarantee support for all
 * PMIx-defined scopes, but should only search data stores within the specified
 * scope within the context of the corresponding "publish" API. The wait flag
 * indicates whether the server should wait for all data to become available
 * before executing the callback function, or should callback with whatever
 * data is immediately available. */
typedef pmix_status_t (*pmix_server_lookup_fn_t)(const char nspace[], int rank,
                                                 pmix_scope_t scope, int wait, char **keys,
                                                 pmix_lookup_cbfunc_t cbfunc, void *cbdata);

/* Delete data from the data store. The host server will be passed a NULL-terminated array
 * of string keys along with the scope within which the data is expected to have
 * been published. The callback is to be executed upon completion of the delete
 * procedure */
typedef pmix_status_t (*pmix_server_unpublish_fn_t)(const char nspace[], int rank,
                                                    pmix_scope_t scope, char **keys,
                                                    pmix_op_cbfunc_t cbfunc, void *cbdata);

/* Spawn a set of applications/processes as per the PMIx API. Note that
 * applications are not required to be MPI or any other programming model.
 * Thus, the host server cannot make any assumptions as to their required
 * support. The callback function is to be executed once all processes have
 * been started. An error in starting any application or process in this
 * request shall cause all applications and processes in the request to
 * be terminated, and an error returned to the originating caller */
typedef pmix_status_t (*pmix_server_spawn_fn_t)(const pmix_app_t apps[], size_t napps,
                                                pmix_spawn_cbfunc_t cbfunc, void *cbdata);

/* Record the specified processes as "connected". This means that the resource
 * manager should treat the failure of any process in the specified group as
 * a reportable event, and take appropriate action. The callback function is
 * to be called once all participating processes have called connect. Note that
 * a process can only engage in *one* connect operation involving the identical
 * set of procs at a time. However, a process *can* be simultaneously engaged
 * in multiple connect operations, each involving a different set of procs */
typedef pmix_status_t (*pmix_server_connect_fn_t)(const pmix_proc_t procs[], size_t nprocs,
                                                  pmix_op_cbfunc_t cbfunc, void *cbdata);

/* Disconnect a previously connected set of processes. An error should be returned
 * if the specified set of procs was not previously "connected". As above, a process
 * may be involved in multiple simultaneous disconnect operations. However, a process
 * is not allowed to reconnect to a set of ranges that has not fully completed
 * disconnect - i.e., you have to fully disconnect before you can reconnect to the
 * same group of processes. */
typedef pmix_status_t (*pmix_server_disconnect_fn_t)(const pmix_proc_t procs[], size_t nprocs,
                                                     pmix_op_cbfunc_t cbfunc, void *cbdata);

/* Callback function for incoming connection requests from
 * local clients */
typedef void (*pmix_connection_cbfunc_t)(int incoming_sd);

/* Register a socket the host server can monitor for connection
 * requests, harvest them, and then call our internal callback
 * function for further processing. A listener thread is essential
 * to efficiently harvesting connection requests from large
 * numbers of local clients such as occur when running on large
 * SMPs. The host server listener is required to call accept
 * on the incoming connection request, and then passing the
 * resulting soct to the provided cbfunc. A NULL for this function
 * will cause the internal PMIx server to spawn its own listener
 * thread */
typedef pmix_status_t (*pmix_server_listener_fn_t)(int listening_sd,
                                                   pmix_connection_cbfunc_t cbfunc);

typedef struct pmix_server_module_1_0_0_t {
    pmix_server_finalized_fn_t        finalized;
    pmix_server_abort_fn_t            abort;
    pmix_server_fencenb_fn_t          fence_nb;
    pmix_server_dmodex_req_fn_t       direct_modex;
    pmix_server_publish_fn_t          publish;
    pmix_server_lookup_fn_t           lookup;
    pmix_server_unpublish_fn_t        unpublish;
    pmix_server_spawn_fn_t            spawn;
    pmix_server_connect_fn_t          connect;
    pmix_server_disconnect_fn_t       disconnect;
    pmix_server_listener_fn_t         listener;
} pmix_server_module_t;

/****    SERVER SUPPORT INIT/FINALIZE FUNCTIONS    ****/

/* Initialize the server support library. The library supports
 * two modes of operation:
 *
 * (a) internal comm - the library will provide all comm
 *     between the server and clients. In this mode, the
 *     caller is only responsible for processing the
 *     data provided in the module callback functions. The
 *     server library will spin off its own progress thread
 *     that will "block" when not actively processing
 *     messages.
 *
 * (b) external comm - the library will provide message
 *     preparation support (e.g., packing/unpacking data),
 *     but all comm between server and clients will be
 *     the responsibility of the caller.
 *
 * Input parameters:
 *
 * module - pointer to a pmix_server_module_t structure
 * containing the caller's callback functions
 *
 * use_internal_comm - boolean that is true if the server
 * library is to use its internal comm system, false if the
 * caller will be providing its own messaging support
 */
pmix_status_t PMIx_server_init(pmix_server_module_t *module,
                               int use_internal_comm);

/* Finalize the server support library. If internal comm is
 * in-use, the server will shut it down at this time. All
 * memory usage is released */
pmix_status_t PMIx_server_finalize(void);

/* retrieve the address and path assigned by the server library for
 * clients to rendezvous with */
pmix_status_t PMIx_get_rendezvous_address(struct sockaddr_un *address, char **path);

/* given a semicolon-separated list of input values, generate
 * a regex that can be passed down to the client for parsing.
 * The caller is responsible for free'ing the resulting
 * string
 *
 * If values have leading zero's, then that is preserved. You
 * have to add back any prefix/suffix for node names
 * odin[009-015,017-023,076-086]
 *
 *     "pmix:odin[009-015,017-023,076-086]"
 *
 * Note that the "pmix" at the beginning of each regex indicates
 * that the PMIx native parser is to be used by the client for
 * parsing the provided regex. Other parsers may be supported - see
 * the pmix_client.h header for a list.
 */
pmix_status_t PMIx_generate_regex(const char *input, char **regex);

/* The input is expected to consist of a comma-separated list
 * of ranges. Thus, an input of:
 *     "1-4;2-5;8,10,11,12;6,7,9"
 * would generate a regex of
 *     "[pmix:2x(3);8,10-12;6-7,9]"
 *
 * Note that the "pmix" at the beginning of each regex indicates
 * that the PMIx native parser is to be used by the client for
 * parsing the provided regex. Other parsers may be supported - see
 * the pmix_client.h header for a list.
 */
pmix_status_t PMIx_generate_ppn(const char *input, char **ppn);

/* Setup the data about a particular nspace so it can
 * be passed to any child process upon startup. The PMIx
 * connection procedure provides an opportunity for the
 * host PMIx server to pass job-related info down to a
 * child process. This might include the number of
 * processes in the job, relative local ranks of the
 * processes within the job, and other information of
 * use to the process. The server is free to determine
 * which, if any, of the supported elements it will
 * provide - defined values are provided in pmix_common.h.
 *
 * NOTE: the server must register ALL nspaces that will
 * participate in collective operations with local processes.
 * This means that the server must register an nspace even
 * if it will not host any local procs from within that
 * nspace IF any local proc might at some point perform
 * a collective operation involving one or more procs from
 * that nspace. This is necessary so that the collective
 * operation can know when it is locally complete.
 *
 * The caller must also provide the number of local procs
 * that will be launched within this nspace. This is required
 * for the PMIx server library to correctly handle collectives
 * as a collective operation call can occur before all the
 * procs have been started */
pmix_status_t PMIx_server_register_nspace(const char nspace[], int nlocalprocs,
                                          pmix_info_t info[], size_t ninfo,
                                          pmix_op_cbfunc_t cbfunc, void *cbdata);

/* Register a client process with the PMIx server library. The
 * expected user ID and group ID of the child process helps the
 * server library to properly authenticate clients as they connect
 * by requiring the two values to match.
 *
 * The host server can also, if it desires, provide an object
 * it wishes to be returned when a server function is called
 * that relates to a specific process. For example, the host
 * server may have an object that tracks the specific client.
 * Passing the object to the library allows the library to
 * return that object when the client calls "finalize", thus
 * allowing the host server to access the object without
 * performing a lookup. */ 
pmix_status_t PMIx_server_register_client(const char nspace[], int rank,
                                          uid_t uid, gid_t gid,
                                          void *server_object,
                                          pmix_op_cbfunc_t cbfunc, void *cbdata);

/* Setup the environment of a child process to be forked
 * by the host so it can correctly interact with the PMIx
 * server. The PMIx client needs some setup information
 * so it can properly connect back to the server. This function
 * will set appropriate environmental variables for this purpose. */
pmix_status_t PMIx_server_setup_fork(const char nspace[],
                                     int rank, char ***env);

/* Define a callback function the PMIx server will use to return
 * direct modex requests to the host server. The PMIx server
 * will free the data blob upon return from the response fn */
typedef void (*pmix_dmodex_response_fn_t)(pmix_status_t status,
                                          char *data, size_t sz,
                                          void *cbdata);

/* Define a function by which the host server can request modex data
 * from the local PMIx server. This is used to support the direct modex
 * operation - i.e., where data is cached locally on each PMIx
 * server for its own local clients, and is obtained on-demand
 * for remote requests. Upon receiving a request from a remote
 * server, the host server will call this function to pass the
 * request into the PMIx server. The PMIx server will return a blob
 * (once it becomes available) via the cbfunc - the host
 * server shall send the blob back to the original requestor */
pmix_status_t PMIx_server_dmodex_request(const char nspace[], int rank,
                                         pmix_dmodex_response_fn_t cbfunc,
                                         void *cbdata);
 
/* Report an error to a process for notification via any
 * registered errhandler. The errhandler registration can be
 * called by both the server and the client application. On the
 * server side, the errhandler is used to report errors detected
 * by PMIx to the host server for handling. On the client side,
 * the errhandler is used to notify the process of errors
 * reported by the server - e.g., the failure of another process.
 *
 * This function allows the host server to direct the server
 * convenience library to notify all indicated local procs of
 * an error. The error can be local, or anywhere in the cluster.
 * The status indicates the error being reported.
 *
 * The first array  of procs informs the server library as to which
 * processes should be alerted - e.g., the processes that are in
 * a directly-affected job or are connected to one that is affected.
 * Passing a NULL for this array will indicate that all local procs
 * are to be notified.
 *
 * The second array identifies the processes that will be impacted
 * by the error. This could consist of a single process, or a number
 * of processes.
 *
 * The info array contains any further info the RM can and/or chooses
 * to provide.
 *
 * If the payload and size parameters are non-NULL, then the function
 * will assume that the caller intends to send the message itself. In
 * this situation, the convenience library will simply pack the message
 * for transmission, and return the payload and size in the provided
 * variables (external comm should have been indicated during server_init).
 * The caller will be responsible for thread protection.
 *
 * Otherwise, the convenience library will transmit the message to
 * the identified target processes, and the function call will be
 * internally thread protected.
 *
 * The callback function will be called upon completion of the
 * notify_error function's actions. Note that any messages will
 * have been queued, but may not have been transmitted by this
 * time. Note that the caller is required to maintain the input
 * data until the callback function has been executed! */
pmix_status_t PMIx_server_notify_error(pmix_status_t status,
                                       pmix_proc_t procs[], size_t nprocs,
                                       pmix_proc_t error_procs[], size_t error_nprocs,
                                       pmix_info_t info[], size_t ninfo,
                                       char **payload, size_t *size,
                                       pmix_op_cbfunc_t cbfunc, void *cbdata);


/****    Message processing for the "lite" version of the  ****
 ****    server convenience library. Note that we must     ****
 ****    preserve the ability of the local server to       ****
 ****    operate asynchronously - thus, the lite library   ****
 ****    cannot block while waiting for a reply to be      ****
 ****    generated                                         ****/

/* retrieve the size of the PMIx message header so the host
 * server messaging system can read the correct number of bytes.
 * Note that the host will need to provide these bytes separately
 * to the message processor */
size_t PMIx_message_hdr_size(void);

/* given the header read by the host server, return the number of
 * bytes the client included in the payload of the message so
 * the host server can read them. These bytes will need to be
 * provided separately to the message processor */
size_t PMIx_message_payload_size(char *hdr);

/* define a callback function by which the convenience library can
 * request that a message be sent via the specified socket. The PMIx
 * header will be included in the provided payload */
typedef void (*pmix_send_message_cbfunc_t)(int sd, void *srv_obj,
                                           char *payload, size_t size);

/* given a socket, conduct the client-server authentication protocol
 * to authenticate the requested connection. The function will return
 * PMIX_SUCCESS if the connection is authenticated, and an appropriate
 * PMIx error code if not. If the client is authenticated, it will
 * be sent whatever initial job_info the host server can provide */
pmix_status_t PMIx_server_authenticate_client(int sd, int *rank,
                                              pmix_send_message_cbfunc_t snd_msg);

/* process a received PMIx client message, sending any desired return
 * via the provided callback function. Params:
 *
 * sd - the socket where the message was received
 * hdr - the header read by the host server messaging system
 * msg - the payload read by the host server messaging system
 * snd_msg - the function to be called when a reply is to be sent */
pmix_status_t PMIx_server_process_msg(int sd, char *hdr, char *msg,
                                      pmix_send_message_cbfunc_t snd_msg);

END_C_DECLS

#endif
