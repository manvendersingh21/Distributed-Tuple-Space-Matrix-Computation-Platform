/*.........................................................................*/
/*                     TSH.C ------> Tuple Space Handler                   */
/*.........................................................................*/

#include "tsh.h"
#include <regex.h>
/*---------------------------------------------------------------------------
  Prototype   : int initCommon(port)
  Parameters  : -
  Returns     : 1 - initialization success
                0 - initialization failed
  Called by   : initFromfile, initFromsocket
  Calls       : signal, getTshport, mapTshport
  Notes       : This function performs required initializations irrespective
                of how TSH is started (DAC/user).
		'oldsock' is a global variable in which TSH connections are
		accepted.
  Date        : April '93
  Coded by    : N. Isaac Rajkumar
  Modification: October '18 Justin Y. Shi for CIS5512
---------------------------------------------------------------------------*/

int initCommon(u_short port)
{
   signal(SIGTERM, sigtermHandler);
				/* get a port to accept requests */
   if ((oldsock = getTshport(htons(port))) == -1)
      return 0 ;
				/* map TSH port with PMD */
				/* initialize tuple space & request queue */
   tsh.space = NULL ;
   tsh.retrieve = NULL;
   tsh.queue_hd = tsh.queue_tl = NULL ;

   return 1 ;
}


/*---------------------------------------------------------------------------
  Prototype   : void start(void)
  Parameters  : -
  Returns     : -
  Called by   : main
  Calls       : get_connection, readn, close, ntohs, appropriate Op-function
  Notes       : This is the controlling function of TSH that invokes
                appropriate routine based on the operation requested.
		The same function 'OpGet' is invoked for both TSH_OP_GET
		& TSH_OP_READ.
  Date        : April '93
  Coded by    : N. Isaac Rajkumar
  Modification:
---------------------------------------------------------------------------*/

void start()
{
   static void (*op_func[])() = {OpPut, OpGet, OpGet, OpExit, Opshell} ;

   while (TRUE)
    {				/* read operation on TSH port */
       if ((newsock = get_connection(oldsock, NULL)) == -1)
	{
	   exit(1) ;
	}
       if (!readn(newsock, (char *)&this_op, sizeof(u_short)))
	{
	   close(newsock) ;
	   continue ;
	}
				/* invoke function for operation */
       this_op = ntohs(this_op) ;

       if (this_op >= TSH_OP_MIN && this_op <= TSH_OP_MAX)
	  (*op_func[this_op - TSH_OP_MIN])() ;

       close(newsock) ;
    }
}
void Opshell()
{
    char output_buffer[BUFFER_SIZE];
    tsh_shell_it in;
    tsh_shell_ot out;
    space1_t *s;
    char *cmd_buf;

    // init response
    out.error  = htons((short)TSH_ER_NOERROR);
    out.status = htons((short)SUCCESS);

    // 1) Read the incoming shell request
    if (!readn(newsock, (char *)&in, sizeof(in))) 
        return;
    in.proc_id = ntohl(in.proc_id);
    if (guardf(in.host, in.proc_id)) 
        return;

    // 2) Allocate and read the command string
    u_long cmd_len = ntohl(in.length);
    cmd_buf = malloc(cmd_len);
    if (!cmd_buf) {
        out.status = htons((short)FAILURE);
        out.error  = htons((short)TSH_ER_NOMEM);
        writen(newsock, (char *)&out, sizeof(out));
        return;
    }
    if (!readn(newsock, cmd_buf, cmd_len)) {
        free(cmd_buf);
        return;
    }

    // 3) Run the shell
    if (run_shell(cmd_buf, output_buffer, sizeof(output_buffer)) != 0) {
        strncpy(out.response, "shell execution failed", sizeof(out.response)-1);
        out.response[sizeof(out.response)-1] = '\0';
        out.status = htons((short)FAILURE);
        writen(newsock, (char *)&out, sizeof(out));
        free(cmd_buf);
        return;
    }

    // 4) Heap‑copy the actual output
    u_long out_len = strlen(output_buffer) + 1;
    char *tuple_data = malloc(out_len);
    if (!tuple_data) {
        free(cmd_buf);
        out.status = htons((short)FAILURE);
        out.error  = htons((short)TSH_ER_NOMEM);
        writen(newsock, (char *)&out, sizeof(out));
        return;
    }
    memcpy(tuple_data, output_buffer, out_len);

    // 5) Create & store the tuple
    s = createTuple(in.name, tuple_data, out_len, ntohs(in.priority));
    free(cmd_buf);
    if (!s) {
        free(tuple_data);
        out.status = htons((short)FAILURE);
        out.error  = htons((short)TSH_ER_NOMEM);
        strncpy(out.response, "error: wrong command", sizeof(out.response)-1);
        out.response[sizeof(out.response)-1] = '\0';
    } else {
        if (!consumeTuple(s)) {
            out.error = htons(storeTuple(s, 0));
        } else {
            out.error = htons((short)TSH_ER_NOERROR);
        }
        out.status = htons((short)SUCCESS);
    }
    memcpy(out.response, output_buffer, out_len);

    // 6) Send the response struct back
    writen(newsock, (char *)&out, sizeof(out));
}


// void Opshell()
// {
//    char output_buffer[BUFFER_SIZE];
//    tsh_shell_it in ;
//    tsh_shell_ot out ;
//    space1_t *s ;
//    char *t ;

//    out.error =  htons((short int)TSH_ER_NOERROR);
//    out.status = htons((short int)SUCCESS);
// // makes sure it prints RIGHT NOW

// 				/* read tuple length, priority, name */
//    if (!readn(newsock, (char *)&in, sizeof(tsh_shell_it)))
//       return ;
//    in.proc_id = ntohl(in.proc_id);
//    if (guardf(in.host, in.proc_id)) return;
// 				/* allocate memory for the tuple */

//    if ((t = (char *)malloc(ntohl(in.length))) == NULL)
//     {
//        out.status = htons((short int)FAILURE) ;
//        out.error = htons((short int)TSH_ER_NOMEM) ;
//        writen(newsock, (char *)&out, sizeof(tsh_shell_ot)) ;
//        return ;
//     }				/* read the tuple */
//    if (!readn(newsock, t, ntohl(in.length)))
//     {
//        free(t) ;
//        return ;
//     }			
//     if(run_shell(t, output_buffer, sizeof(output_buffer)) == 0) {
//       printf("Captured output: %s\n", output_buffer);
//   }
//     /* create and store tuple in space */
//     strcpy(out.response, output_buffer);
//    s = createTuple(in.name, output_buffer, ntohl(in.length), ntohs(in.priority)) ;
//    if (s == NULL)
//     {
//        free(t) ;
//        out.status = htons((short int)FAILURE) ;
//        out.error = htons((short int)TSH_ER_NOMEM) ;
//        strncpy(out.response, "error , wrong command", 100);
//     }
//    else
//     {			/* satisfy pending requests, if possible */
//       free(t);
//        if (!consumeTuple(s)) {
// 	  out.error = htons(storeTuple(s, 0)) ;
//        } else {
//           out.error = htons((short int)TSH_ER_NOERROR) ;
//        }
//        out.status = htons((short int)SUCCESS) ;
//     }
    
//    writen(newsock, (char *)&out, sizeof(tsh_shell_ot)) ;

// }


/*---------------------------------------------------------------------------
  Prototype   : void OpPut(void)
  Parameters  : -
  Returns     : -
  Called by   : start
  Calls       : createTuple, consumeTuple, storeTuple, readn, writen,
                ntohs, ntohl, malloc, free
  Notes       : A tuple is created based on the data received. If there are
                pending requests for this tuple they are processed. If the
		tuple is not consumed by them (i.e. no GET) the tuple is
		stored in the tuple space.
  Date        : April '93
  Coded by    : N. Isaac Rajkumar
  Modification:
---------------------------------------------------------------------------*/

void OpPut()
{
   tsh_put_it in ;
   tsh_put_ot out ;
   space1_t *s ;
   char *t ;

   out.error =  htons((short int)TSH_ER_NOERROR);
   out.status = htons((short int)SUCCESS);
// makes sure it prints RIGHT NOW

				/* read tuple length, priority, name */
   if (!readn(newsock, (char *)&in, sizeof(tsh_put_it)))
      return ;
   in.proc_id = ntohl(in.proc_id);
   if (guardf(in.host, in.proc_id)) return;
				/* allocate memory for the tuple */

   if ((t = (char *)malloc(ntohl(in.length))) == NULL)
    {
       free(t) ;
       out.status = htons((short int)FAILURE) ;
       out.error = htons((short int)TSH_ER_NOMEM) ;
       writen(newsock, (char *)&out, sizeof(tsh_put_ot)) ;
       return ;
    }				/* read the tuple */
   if (!readn(newsock, t, ntohl(in.length)))
    {
       free(t) ;
       return ;
    }				/* create and store tuple in space */
   s = createTuple(in.name, t, ntohl(in.length), ntohs(in.priority)) ;
   if (s == NULL)
    {
       free(t) ;
       out.status = htons((short int)FAILURE) ;
       out.error = htons((short int)TSH_ER_NOMEM) ;
    }
   else
    {			/* satisfy pending requests, if possible */
       if (!consumeTuple(s)) {
	  out.error = htons(storeTuple(s, 0)) ;
       } else {
          out.error = htons((short int)TSH_ER_NOERROR) ;
       }
       out.status = htons((short int)SUCCESS) ;
    }
   writen(newsock, (char *)&out, sizeof(tsh_put_ot)) ;
}


/*---------------------------------------------------------------------------
  Prototype   : void OpGet(void)
  Parameters  : -
  Returns     : -
  Called by   : start
  Calls       : findTuple, deleteTuple, storeRequest, readn, writen,
                strcpy, htons
  Notes       : This function is called for both TSH_OP_READ and TSH_OP_GET.
                If the tuple is present in the tuple space it is returned,
		or else the request is queued.
  Date        : April '93
  Coded by    : N. Isaac Rajkumar
  Modification:
---------------------------------------------------------------------------*/

void OpGet()
{
   tsh_get_it in ;
   tsh_get_ot1 out1 ;
   tsh_get_ot2 out2 ;
   space1_t *s ;
   int request_len;
				/* read tuple name */
   if (!readn(newsock, (char *)&in, sizeof(tsh_get_it)))
      return ;
   in.proc_id = ntohl(in.proc_id);
   if (guardf(in.host, in.proc_id)) return;
   request_len = ntohl(in.len); /* get user requested length */
				/* locate tuple in tuple space */
   if ((s = findTuple(in.expr)) == NULL)
    {
       out1.status = htons(FAILURE) ;
       if (request_len != -1) /* -1: async read/get. Do not queue */
       {
          if (!storeRequest(in))
	     out1.error = htons(TSH_ER_NOMEM) ;
          else
	     out1.error = htons(TSH_ER_NOTUPLE) ;
       }
       writen(newsock, (char *)&out1, sizeof(tsh_get_ot1)) ;
       return ;
    }
				/* report that tuple exists */
   out1.status = htons(SUCCESS) ; out1.error = htons(TSH_ER_NOERROR) ;
   if (!writen(newsock, (char *)&out1, sizeof(tsh_get_ot1)))
      return ;
				/* send tuple name, length and priority */
   strcpy(out2.name, s->name) ;
   if ((s->length > request_len) && (request_len != 0))
        out2.length = in.len;
   else out2.length = htonl(s->length) ;
   out2.priority = htons(s->priority) ;
   if (!writen(newsock, (char *)&out2, sizeof(tsh_get_ot2)))
      return ;
				/* send the tuple */
   if (!writen(newsock, s->tuple, ntohl(out2.length) /*s->length*/))
      return ;

   if (this_op == TSH_OP_GET)
      deleteTuple(s, &in) ;
}


/*---------------------------------------------------------------------------
  Prototype   : void OpExit(void)
  Parameters  : -
  Returns     : -
  Called by   : tsh_start
  Calls       : deleteSpace, deleteQueue, unmapTshport, writen, exit
  Notes       : This function clears up the tuple space when TSH_OP_EXIT
                is sent by DAC.
                This operation is received only when TSH is started by CID.
  Date        : April '93
  Coded by    : N. Isaac Rajkumar
  Modification:
---------------------------------------------------------------------------*/

void OpExit()
{
   tsh_exit_ot out ;
				/* report successful TSH exit */
   out.status = htons(SUCCESS) ;  out.error = htons(TSH_ER_NOERROR) ;
   writen(newsock, (char *)&out, sizeof(tsh_exit_ot)) ;

   deleteSpace() ;		/* delete all tuples, requests */
   deleteQueue() ;

//   unmapTshport() ;		/* unmap TSH port from PMD */
   exit(NORMAL_EXIT) ;
}


/*---------------------------------------------------------------------------
  Prototype   : void deleteSpace(void)
  Parameters  : -
  Returns     : -
  Called by   : OpExit, sigtermHandler
  Calls       : free
  Notes       : This function frees all the memory associated with tuple
                space. It's invoked when the TSH has to exit.
  Date        : April '93
  Coded by    : N. Isaac Rajkumar
  Modification:
---------------------------------------------------------------------------*/

void deleteSpace()
{
   space1_t *s ;
   space2_t *p_q;

   while (tsh.space != NULL)
    {
       s = tsh.space ;
       tsh.space = tsh.space->next ;
       free(s->tuple) ;		/* free tuple, tuple node */
       free(s) ;
    }
   while (tsh.retrieve != NULL)
    {
       p_q = tsh.retrieve;
       tsh.retrieve = tsh.retrieve->next;
       free(p_q->tuple);
       free(p_q);
    }
}


/*---------------------------------------------------------------------------
  Prototype   : void deleteQueue(void)
  Parameters  : -
  Returns     : -
  Called by   : sigtermHandler, OpExit
  Calls       : free
  Notes       : This function frees all memory associated with the pending
                requests. It's invoked when the TSH has to exit.
  Date        : April '93
  Coded by    : N. Isaac Rajkumar
  Modification:
---------------------------------------------------------------------------*/

void deleteQueue()
{
   queue1_t *q ;

   while (tsh.queue_hd != NULL)
    {
       q = tsh.queue_hd ;
       tsh.queue_hd = tsh.queue_hd->next ;
       free(q) ;		/* free request node */
    }
}


/*---------------------------------------------------------------------------
  Prototype   : int consumeTuple(space1_t *s)
  Parameters  : s - pointer to tuple that has to be consumed
  Returns     : 1 - tuple consumed
                0 - tuple not consumed
  Called by   : OpPut
  Calls       : findRequest, sendTuple, deleteRequest
  Notes       : If there is a pending request that matches this tuple, it
                is sent to the requestor (served FIFO). If there were only
		pending TSH_OP_READs (or no pending requests) then the tuple
		is considered not consumed. If a TSH_OP_GET was encountered
		then the tuple is considered consumed.
  Date        : April '93
  Coded by    : N. Isaac Rajkumar
  Modification: FSUN 10/94. Move the tuple consumed by a request to retrieve
		list, For FDD.
---------------------------------------------------------------------------*/

int consumeTuple(space1_t *s)
{
   queue1_t *q ;
   space2_t *p_q;
				/* check whether request pending */
   if ((q = findRequest(s->name)) != NULL)
    {
		do {		/* send tuple to requestor, delete request */
//			printf ("tsh: found matching request (%s) \n",s->name);
			if (sendTuple(q, s) > 0)
				if (q->request == TSH_OP_GET)
				{
					deleteRequest(q) ;
					/* add the tuple into backup queue. FSUN 10/94. */
					p_q = tsh.retrieve;
					while (p_q != NULL) {
						if (p_q->host == q->host && p_q->proc_id == q->proc_id) {
							strcpy(p_q->name, s->name);
							p_q->port = q->port;
							p_q->cidport = q->cidport;  /* for dspace ys'96 */
/*
printf(" TSH captured host(%ul) port(%d) tpname(%s)\n", q->host, q->cidport,
			s->name);
*/
							total_fetched ++;
							p_q->length = s->length;
							p_q->priority = s->priority;
							free(p_q->tuple);
							p_q->tuple = s->tuple;
							return 1;
                        }
                        p_q = p_q->next;
					}
/*
printf(" TSH captured host(%ul) port(%d) tpname(%s)\n", q->host, q->cidport,
			s->name);
*/
					total_fetched ++;
/*
printf(" TSH. fetched (%d)\n", total_fetched);
*/
					p_q = (space2_t *)malloc(sizeof(space2_t));
					p_q->host = q->host;
					p_q->port = q->port;
					p_q->cidport = q->cidport;  /* for dspace ys'96 */
/*
printf(" TSH captured new host(%ul) port(%d) tpname(%s)\n", q->host, q->cidport,
			s->name);
*/
					p_q->proc_id = q->proc_id;
					strcpy(p_q->name, s->name);
					p_q->length = s->length;
					p_q->priority = s->priority;
					p_q->fault = 0;
					p_q->tuple = s->tuple;
					p_q->next = tsh.retrieve;
					tsh.retrieve = p_q;
					free(s);
					return 1 ;	/* tuple consumed */
				}
				deleteRequest(q) ;
				/* check for another pending request */
		} while ((q = findRequest(s->name)) != NULL) ;
    }
	return 0 ;			/* tuple not consumed */
}


/*---------------------------------------------------------------------------
  Prototype   : space1_t *createTuple(char *name, u_long length,
                                                           u_short priority)
  Parameters  : name     - tuple name
                length   - length of tuple
		priority - priority of the tuple
  Returns     : pointer to a tuple made of the input [or] NULL if no memory
  Called by   : OpPut
  Calls       : malloc, strcpy
  Notes       : This function creates a tuple and fills up the attributes.
  Date        : April '93
  Coded by    : N. Isaac Rajkumar
  Modification:
---------------------------------------------------------------------------*/

space1_t *createTuple(char *name, char *tuple, u_long length, u_short priority)
{
   space1_t *s ;
				/* create a new node and store tuple */
   if ((s = (space1_t *) malloc(sizeof(space1_t))) == NULL)
      return NULL ;
   strcpy(s->name, name) ;
   s->length = length ;
   s->tuple = tuple ;
   s->priority = priority ;

   return s ;			/* return new tuple */
}


/*---------------------------------------------------------------------------
  Prototype   : short int storeTuple(space1_t *s)
  Parameters  : s - pointer to tuple that has to be stored
  Returns     : -
  Called by   : OpPut
  Calls       : strcmp, free
  Notes       : The tuple is stored in the tuple space. If another tuple
                exists with the same name it is replaced.
  Date        : April '93
  Coded by    : N. Isaac Rajkumar
  Modification: Made FIFO from LIFO.
---------------------------------------------------------------------------*/

short int storeTuple(space1_t *s, int f)
{
   space1_t *ptr ;
				/* check if tuple already there */
   for (ptr = tsh.space ; ptr != NULL ; ptr = ptr->next)
    {
       if (!strcmp(ptr->name, s->name))
	{			/* overwrite existing tuple */
	   free(ptr->tuple) ;
	   ptr->tuple = s->tuple ;
	   ptr->length = s->length ;
	   ptr->priority = s->priority ;
	   free(s) ;

	   return ((short int)TSH_ER_OVERRT) ;
	}
       if (ptr->next == NULL)
	  break ;
    }
        if (f == 0) {           /* add tuple to end of space */
                s->next = NULL ;
                s->prev = ptr ;
                if (ptr == NULL)
                        tsh.space = s ;
                else
                        ptr->next = s ;
        } else {        /* add tuple retrieved to header of space */
                s->next = tsh.space;
                s->prev = NULL;
                tsh.space = s;
        }
   return((short int)TSH_ER_NOERROR);
}


/*---------------------------------------------------------------------------
  Prototype   : space1_t *findTuple(char *expr)
  Parameters  : expr - wildcard expression (*, ?)
  Returns     : pointer to tuple in the tuple space
  Called by   : OpGet
  Calls       : match
  Notes       : The tuple matching the wildcard expression & with the
                highest priority of all the matches is determined and
		returned.
  Date        : April '93
  Coded by    : N. Isaac Rajkumar
  Modification:
---------------------------------------------------------------------------*/

space1_t *findTuple(char *expr)
{
   space1_t *high = NULL, *s ;
				/* search tuple space for highest priority */
   for (s = tsh.space ; s != NULL ; s = s->next)
   {
      if (match(expr, s->name) && ((high == NULL) ||
				   (s->priority > high->priority)))
      {
	 high = s ;
      }
   }
   return high ;		/* return tuple or NULL if no match */
}


/*---------------------------------------------------------------------------
  Prototype   : void deleteTuple(space1_t *s)
  Parameters  : s - pointer to tuple to be deleted from tuple space
  Returns     : -
  Called by   : OpGet
  Calls       : free
  Notes       : The tuple is removed from the tuple space.
  Date        : April '93
  Coded by    : N. Isaac Rajkumar
  Modification: Added Fault Toloerance Function. FSUN 10/94.
---------------------------------------------------------------------------*/
void deleteTuple(space1_t *s, tsh_get_it *r)
{
        space2_t *p_q;

        if (s == tsh.space)             /* remove tuple from space */
                tsh.space = s->next ;
        else
                s->prev->next = s->next ;

        if (s->next != NULL) s->next->prev = s->prev  ;

        /* add the tuple into backup queue. FSUN 10/94. */
        p_q = tsh.retrieve;
        while (p_q != NULL) {
                if (p_q->host == r->host && p_q->proc_id == r->proc_id) {
                        strcpy(p_q->name, s->name);
        		p_q->port = r->port;
			p_q->cidport = r->cidport;  /* for dspace. ys'96 */
/*
printf(" TSH: Captured host(%ul) port(%d) tpname(%s)\n", p_q->host,
			p_q->cidport, p_q->name);
*/
                        p_q->length = s->length;
                        p_q->priority = s->priority;
                        free(p_q->tuple);
                        p_q->tuple = s->tuple;
                        return;
                }
                p_q = p_q->next;
        }
        p_q = (space2_t *)malloc(sizeof(space2_t));
        p_q->host = r->host;
        p_q->port = r->port;
	p_q->cidport = r->cidport;   /* for dspace. ys'96 */
	p_q->proc_id = r->proc_id;
        strcpy(p_q->name, s->name);
/*
printf(" TSH: Captured new host(%ul) port(%d) tpname(%s)\n", p_q->host,
			p_q->cidport, p_q->name);
*/
        p_q->length = s->length;
        p_q->priority = s->priority;
        p_q->fault = 0;
        p_q->tuple = s->tuple;
	p_q->next = tsh.retrieve;
	tsh.retrieve = p_q;
	free(s);
}


/*---------------------------------------------------------------------------
  Prototype   : int sendTuple(queue1_t *q, space1_t *s)
  Parameters  : q - pointer to pending request entry in the queue
                s - pointer to the tuple to be sent
  Returns     : 1 - tuple successfully sent
                0 - tuple not sent
  Called by   : consumeTuple
  Calls       : get_socket, do_connect, writen, close
  Notes       : The tuple is sent to the host/port specified in the request.
  Date        : April '93
  Coded by    : N. Isaac Rajkumar
  Modification:
---------------------------------------------------------------------------*/

int sendTuple(queue1_t *q, space1_t *s)
{
   tsh_get_ot2 out ;
   int sd ;
				/* connect to the requestor */
//   printf("Sending tuple to requester (%s) host(%lu) port(%d)\n", s->name, q->host, q->port);
   if ((sd = get_socket()) == -1)
   {
	printf(" Cannot getsocket for tuple object. Call system operator. \n");
	exit (E_SOCKET);
   }

   if (!do_connect(sd, q->host, q->port))
    {
       close(sd) ;
       return (E_CONNECT);
    }
 				/* send tuple name, length, priority */
   strcpy(out.name, s->name) ;
   out.priority = htons(s->priority) ;
   out.length = htonl(s->length) ;
   if (!writen(sd, (char *)&out, sizeof(tsh_get_ot2)))
    {
       close(sd) ;
       return (E_CONNECT);
    }				/* send tuple data */
   if (!writen(sd, s->tuple, s->length))
    {
       close(sd) ;
       return (E_CONNECT);
    }
   close(sd);
   return 1 ;			/* tuple successfully sent */
}


/*---------------------------------------------------------------------------
  Prototype   : queue_t *findRequest(char *name)
  Parameters  : name - tuple name
  Returns     : pointer to a request for this tuple
                NULL if no request for this tuple
  Called by   : consumeTuple
  Calls       : match
  Notes       : If there is a pending request for the specified tuple, it is
                returned.
  Date        : April '93
  Coded by    : N. Isaac Rajkumar
  Modification:
---------------------------------------------------------------------------*/

queue1_t *findRequest(char *name)
{
   queue1_t *q ;
				/* search pending request queue */
   for (q = tsh.queue_hd ; q != NULL ; q = q->next)
   {
      if (match(q->expr, name))
      {
	 return q ;		/* determine request that matches tuple */
      }
   }

   return NULL ;
}


/*---------------------------------------------------------------------------
  Prototype   : void deleteRequest(queue1_t *q)
  Parameters  : q - pointer to request in queue
  Returns     : -
  Called by   : OpPut
  Calls       : free
  Notes       : The specified request is removed from the pending requests
                queue.
  Date        : April '93
  Coded by    : N. Isaac Rajkumar
  Modification:
---------------------------------------------------------------------------*/

void deleteRequest(queue1_t *q)
{				/* remove request from queue */
   if (q == tsh.queue_hd)
      tsh.queue_hd = q->next ;
   else
      q->prev->next = q->next ;

   if (q == tsh.queue_tl)
      tsh.queue_tl = q->prev ;
   else
      q->next->prev = q->prev ;

   free(q) ;			/* free request */
}


/*---------------------------------------------------------------------------
  Prototype   : int storeRequest(tsh_get_it in)
  Parameters  : expr - name (wildcard expression) of the requested tuple
                host - address of the reque*stor
		port - port at which the tuple has to be delivered
		cidport - the cid of the requester's host
  Returns     : 1 - request stored
                0 - no space to store request
  Called by   : OpGet
  Calls       : malloc, strcpy
  Notes       : A request based on the parameters is placed in the pending
                request queue.
  Date        : April '93
  Coded by    : N. Isaac Rajkumar
  Modification: FSUN 10/94. Added proc_id in the request stored for FDD.
---------------------------------------------------------------------------*/

int storeRequest(tsh_get_it in)
{
   queue1_t *q ;
				/* create node for request */
   if ((q = (queue1_t *)malloc(sizeof(queue1_t))) == NULL)
      return 0 ;
   strcpy(q->expr, in.expr) ;
   q->port = in.port ;
   q->cidport = in.cidport ;  /* for dspace. ys'96 */
   q->host = in.host ;
   q->proc_id = in.proc_id;
   q->request = this_op ;
   q->next = NULL ;
   q->prev = tsh.queue_tl ;
				/* store request in queue */
   if (tsh.queue_tl == NULL)
      tsh.queue_hd = q ;
   else
      tsh.queue_tl->next = q ;
   tsh.queue_tl = q ;
   return 1 ;
}


/*---------------------------------------------------------------------------
  Prototype   : int getTshport(void)
  Parameters  : -
  Returns     : socket descriptor of the port allocated to TSH
  Called by   : initCommon
  Calls       : get_socket, bind_socket, close
  Notes       : A socket is obtained and bound to a port. The TSH waits for
                requests on this port.
  Date        : April '93
  Coded by    : N. Isaac Rajkumar
  Modification:
---------------------------------------------------------------------------*/

int getTshport(u_short port)
{
   int sd ;
				/* obtain socket with required properties */
   if ((sd = get_socket()) == -1)
      return -1 ;		/* bind socket to the port */
   if (!(tsh.port = bind_socket(sd, htons(port))))
    {
       close(sd) ;
       return -1 ;
    }				/* allow connections to TSH on this socket*/
   return sd ;			/* return socket for TSH */
}


/*---------------------------------------------------------------------------
  Prototype   : int match(char *expr, char *name)
  Parameters  : expr - wildcard expression
                name - string to be matched with the wildcard
  Returns     : 1 - expr & name match
                0 - expr & name do not match
  Called by   : match, findRequest, findTuple
  Calls       : match
  Notes       : The expression can contain wild card characters '*' and '?'.
                I like this function.
  Date        : April '93
  Coded by    : N. Isaac Rajkumar
  Modification:
---------------------------------------------------------------------------*/

int match(char *expr, char *name){
	regex_t preg;
  int rc;

  if (0 != (rc = regcomp(&preg, expr, 0))) {
     printf("regcomp() failed, returning nonzero (%d)\n", rc);
     exit(EXIT_FAILURE);
  }

  if (0 != (rc = regexec(&preg, name, 0, NULL, 0))) {
    return 0; 			/* no match found */
  }
  else {
   //   printf("match found (%s) in (%s)\n", expr, name);
     return 1;
  }
}


/*---------------------------------------------------------------------------
  Prototype   : void sigtermHandler(void)
  Parameters  : -
  Returns     : -
  Called by   : By system on SIGTERM
  Calls       : deleteSpace, deleteQueue, unmapTshport, exit
  Notes       : This function is invoked when TSH is terminated by the user
                i.e. when TSH is started by the user and not by CID.
		This is the right way to kill TSH when it is started by user
		(i.e. by sending SIGTERM).
  Date        : April '93
  Coded by    : N. Isaac Rajkumar
  Modification:
---------------------------------------------------------------------------*/

void sigtermHandler()
{
   deleteSpace() ;		/* delete all tuples, requests */
   deleteQueue() ;
   exit(0) ;
}




/*---------------------------------------------------------------------------
  Prototype   : void guardf(u_long hostid, int procid)
  Parameters  : -
  Returns     : -
  Called by   :
  Calls       :
  Notes       :
  Date        : October '94
  Coded by    : Feijian Sun
  Modification:
---------------------------------------------------------------------------*/

int guardf(u_long hostid, int procid)
{
        space2_t *p_q;

        p_q = tsh.retrieve;
        while (p_q != NULL) {
                if (p_q->fault == 1 && p_q->host == hostid
				&& p_q->proc_id == procid)
                        return(1);
		p_q = p_q->next;
	}
        return(0);
}


/*---------------------------------------------------------------------------
  Prototype   : int main(int argc, char **argv)
  Parameters  : argv[1] -  "-s" --> read initialization data from socket [or]
                           "-a" --> read initialization data from command line
		argv[2] -  socket descriptor #
		           application id
		argv[3] -  TSH name
  Returns     : Never returns
  Called by   : System
  Calls       : initFromsocket, initFromline, start, strcmp, exit
  Notes       : TSH can be started either by CID or from the shell prompt
                by the user. In the former case, initialization data is
		read from the socket (from DAC). Otherwise, data is read
		from the command line. The switch -s or -a indicate the option.
  Date        : April '93
  Coded by    : N. Isaac Rajkumar
  Modification: Modified TSH to read appid, name from command line.
		February '13, updated by Justin Y. Shi
---------------------------------------------------------------------------*/

int main(int argc, char **argv)
{
   if (argc < 2)
   {
      printf("Usage: tsh port &\n");
      exit(1) ;
   }
   // Insert env variable check to obtain TSHPORT from .bashrc
   if (!initCommon(atoi(argv[1]))) {
	   printf("Port(%s) is in use. Please try a different number", argv[1]);
	   exit(1);
   }
   start() ;
}

int run_shell(char* input, char* output_buffer, size_t buffer_size) {
   int pipefd[2];
   pid_t pid;
   char buffer[buffer_size];
   memset(buffer, 0, sizeof(buffer));

   if (pipe(pipefd) == -1) {
       perror("pipe");
       return 1;
   }

   pid = fork();

   if (pid == -1) {
       perror("fork");
       return 1;
   } else if (pid == 0) {
       // Child process
       close(pipefd[0]); // Close unused read end
       dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to pipe
       dup2(pipefd[1], STDERR_FILENO); // Optional: redirect stderr too
       execute_command(input);  // Run the command
       close(pipefd[1]);

       // If execvp fails
       perror("execvp");
       exit(1);
   } else {
       // Parent process
       close(pipefd[1]); // Close unused write end
       int bytes_read = read(pipefd[0], buffer, buffer_size - 1); // Read output
       close(pipefd[0]);

       if (bytes_read > 0) {
           strncpy(output_buffer, buffer, bytes_read); // Copy to output_buffer
           output_buffer[bytes_read] = '\0';  // Null terminate the string
       } else {
           output_buffer[0] = '\0';  // If nothing was read, set an empty string
       }

       waitpid(pid, NULL, 0); // Wait for the child process to finish
   }

   return 0;
}

void execute_command(char *input) {
   char *args[MAX_ARG_COUNT];
   char *token;
   int i = 0;
   int pipefd[2];
   pid_t pid1, pid2;
   int output_redirect = 0;
   char *output_file = NULL;

   // Check if there is a pipe in the input
   char *pipe_pos = strchr(input, '|');
   char *redirect_pos = strchr(input, '>');

   // Handle output redirection (>)
   if (redirect_pos != NULL) {
       *redirect_pos = '\0'; // Split the input at '>'
       output_file = strtok(redirect_pos + 1, " \n"); // Get the filename
       output_redirect = 1; // Mark that we have a redirection
   }

   // If there is a pipe, handle it
   if (pipe_pos != NULL) {
       // Split the input into two parts before and after the pipe
       *pipe_pos = '\0';
       pipe_pos++;

       // Create the pipe
       if (pipe(pipefd) == -1) {
           perror("Pipe failed");
           return;
       }

       // Parse the first command (before pipe)
       token = strtok(input, " \n");
       i = 0;
       while (token != NULL) {
           args[i++] = token;
           token = strtok(NULL, " \n");
       }
       args[i] = NULL;

       // Fork the first child to execute the first command
       pid1 = fork();
       if (pid1 == 0) {
           // Child process for the first command
           close(pipefd[0]); // Close unused read end of the pipe
           dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to pipe
           close(pipefd[1]); // Close write end of the pipe after duplicating

           if (execvp(args[0], args) == -1) {
               perror("Execution failed");
               exit(1);
           }
       }

       // Parse the second command (after pipe)
       i = 0;
       token = strtok(pipe_pos, " \n");
       while (token != NULL) {
           args[i++] = token;
           token = strtok(NULL, " \n");
       }
       args[i] = NULL;

       // Fork the second child to execute the second command
       pid2 = fork();
       if (pid2 == 0) {
           // Child process for the second command
           close(pipefd[1]); // Close unused write end of the pipe
           dup2(pipefd[0], STDIN_FILENO); // Redirect stdin from pipe
           close(pipefd[0]); // Close read end of the pipe after duplicating

           if (output_redirect) {
               // Redirect output to the file
               int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
               if (fd == -1) {
                   perror("Failed to open file for output redirection");
                   exit(1);
               }
               dup2(fd, STDOUT_FILENO); // Redirect stdout to file
               close(fd); // Close the file descriptor
           }

           if (execvp(args[0], args) == -1) {
               perror("Execution failed");
               exit(1);
           }
       }

       // Parent process waits for both children to finish
       close(pipefd[0]);
       close(pipefd[1]);
       waitpid(pid1, NULL, 0);
       waitpid(pid2, NULL, 0);
   } else {
       // No pipe, just execute a single command
       token = strtok(input, " \n");
       i = 0;
       while (token != NULL) {
           args[i++] = token;
           token = strtok(NULL, " \n");
       }
       args[i] = NULL;

       // Fork a child process to execute the command
       pid_t pid = fork();
       if (pid == 0) {
           // Child process

           if (output_redirect) {
               // Redirect output to the file
               int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
               if (fd == -1) {
                   perror("Failed to open file for output redirection");
                   exit(1);
               }
               dup2(fd, STDOUT_FILENO); // Redirect stdout to file
               close(fd); // Close the file descriptor
           }

           if (execvp(args[0], args) == -1) {
               perror("Execution failed");
               exit(1);
           }
       } else if (pid > 0) {
           // Parent process waits for the child to finish
           wait(NULL);
       } else {
           // Fork failed
           perror("Fork failed");
       }
   }
}