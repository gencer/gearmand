/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011 Data Differential, http://datadifferential.com/
 *  Copyright (C) 2008 Brian Aker, Eric Day
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *      * Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above
 *  copyright notice, this list of conditions and the following disclaimer
 *  in the documentation and/or other materials provided with the
 *  distribution.
 *
 *      * The names of its contributors may not be used to endorse or
 *  promote products derived from this software without specific prior
 *  written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/**
 * @file
 * @brief Packet Definitions
 */

#include <libgearman/common.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <memory>

/**
 * @addtogroup gearman_packet_static Static Packet Declarations
 * @ingroup gearman_packet
 * @{
 */

/**
 * Command info. Update GEARMAN_MAX_COMMAND_ARGS to the largest number in the
 * args column.
 */
gearman_command_info_st gearman_command_info_list[GEARMAN_COMMAND_MAX]=
{
  { "TEXT",               3, false },
  { "CAN_DO",             1, false },
  { "CANT_DO",            1, false },
  { "RESET_ABILITIES",    0, false },
  { "PRE_SLEEP",          0, false },
  { "UNUSED",             0, false },
  { "NOOP",               0, false },
  { "SUBMIT_JOB",         2, true  },
  { "JOB_CREATED",        1, false },
  { "GRAB_JOB",           0, false },
  { "NO_JOB",             0, false },
  { "JOB_ASSIGN",         2, true  },
  { "WORK_STATUS",        3, false },
  { "WORK_COMPLETE",      1, true  },
  { "WORK_FAIL",          1, false },
  { "GET_STATUS",         1, false },
  { "ECHO_REQ",           0, true  },
  { "ECHO_RES",           0, true  },
  { "SUBMIT_JOB_BG",      2, true  },
  { "ERROR",              2, false },
  { "STATUS_RES",         5, false },
  { "SUBMIT_JOB_HIGH",    2, true  },
  { "SET_CLIENT_ID",      1, false },
  { "CAN_DO_TIMEOUT",     2, false },
  { "ALL_YOURS",          0, false },
  { "WORK_EXCEPTION",     1, true  },
  { "OPTION_REQ",         1, false },
  { "OPTION_RES",         1, false },
  { "WORK_DATA",          1, true  },
  { "WORK_WARNING",       1, true  },
  { "GRAB_JOB_UNIQ",      0, false },
  { "JOB_ASSIGN_UNIQ",    3, true  },
  { "SUBMIT_JOB_HIGH_BG", 2, true  },
  { "SUBMIT_JOB_LOW",     2, true  },
  { "SUBMIT_JOB_LOW_BG",  2, true  },
  { "SUBMIT_JOB_SCHED",   7, true  },
  { "SUBMIT_JOB_EPOCH",   3, true  }
};

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

inline static gearman_return_t packet_create_arg(gearman_packet_st *packet,
                                                 const void *arg, size_t arg_size)
{
  void *new_args;

  if (packet->argc == gearman_command_info_list[packet->command].argc and
      (! (gearman_command_info_list[packet->command].data) || packet->data != NULL))
  {
    gearman_universal_set_error(packet->universal, GEARMAN_TOO_MANY_ARGS, AT, "too many arguments for command (%s)",
                                gearman_command_info_list[packet->command].name);
    return GEARMAN_TOO_MANY_ARGS;
  }

  if (packet->argc == gearman_command_info_list[packet->command].argc)
  {
    packet->data= arg;
    packet->data_size= arg_size;
    return GEARMAN_SUCCESS;
  }

  if (packet->args_size == 0 && packet->magic != GEARMAN_MAGIC_TEXT)
    packet->args_size= GEARMAN_PACKET_HEADER_SIZE;

  if ((packet->args_size + arg_size) < GEARMAN_ARGS_BUFFER_SIZE)
  {
    packet->args= packet->args_buffer;
  }
  else
  {
    if (packet->args == packet->args_buffer)
      packet->args= NULL;

    new_args= realloc(packet->args, packet->args_size + arg_size);
    if (new_args == NULL)
    {
      gearman_perror(packet->universal, "packet realloc");
      return GEARMAN_MEMORY_ALLOCATION_FAILURE;
    }

    if (packet->args_size > 0)
      memcpy(new_args, packet->args_buffer, packet->args_size);

    packet->args= (char *)new_args;
  }

  memcpy(packet->args + packet->args_size, arg, arg_size);
  packet->args_size+= arg_size;
  packet->arg_size[packet->argc]= arg_size;
  packet->argc++;

  size_t offset;
  if (packet->magic == GEARMAN_MAGIC_TEXT)
  {
    offset= 0;
  }
  else
  {
    offset= GEARMAN_PACKET_HEADER_SIZE;
  }

  for (uint8_t x= 0; x < packet->argc; x++)
  {
    packet->arg[x]= packet->args + offset;
    offset+= packet->arg_size[x];
  }

  return GEARMAN_SUCCESS;
}

/** @} */

/*
 * Public Definitions
 */


gearman_packet_st *gearman_packet_create(gearman_universal_st *gearman,
                                         gearman_packet_st *packet)
{
  if (packet == NULL)
  {
    packet= new (std::nothrow) gearman_packet_st;
    if (packet == NULL)
    {
      gearman_perror(gearman, "gearman_packet_st malloc");
      errno= ENOMEM;
      return NULL;
    }

    packet->options.allocated= true;
  }
  else
  {
    packet->options.allocated= false;
    packet->options.complete= false;
    packet->options.free_data= false;
  }

  packet->magic= GEARMAN_MAGIC_TEXT;
  packet->command= GEARMAN_COMMAND_TEXT;
  packet->argc= 0;
  packet->args_size= 0;
  packet->data_size= 0;
  packet->universal= gearman;

  if (! (gearman->options.dont_track_packets))
  {
    if (gearman->packet_list != NULL)
      gearman->packet_list->prev= packet;
    packet->next= gearman->packet_list;
    packet->prev= NULL;
    gearman->packet_list= packet;
    gearman->packet_count++;
  }

  packet->args= NULL;
  packet->data= NULL;

  return packet;
}

gearman_return_t gearman_packet_create_arg(gearman_packet_st *packet,
                                           const void *arg, size_t arg_size)
{
  return packet_create_arg(packet, arg, arg_size);
}

gearman_return_t gearman_packet_create_args(gearman_universal_st *gearman,
                                            gearman_packet_st *packet,
                                            enum gearman_magic_t magic,
                                            gearman_command_t command,
                                            const void *args[],
                                            const size_t args_size[],
                                            size_t args_count)
{
  packet= gearman_packet_create(gearman, packet);
  if (packet == NULL)
  {
    return GEARMAN_MEMORY_ALLOCATION_FAILURE;
  }

  packet->magic= magic;
  packet->command= command;

  for (size_t x= 0; x < args_count; x++)
  {
    gearman_return_t ret= packet_create_arg(packet, args[x], args_size[x]);
    if (gearman_failed(ret))
    {
      gearman_packet_free(packet);
      return ret;
    }
  }

  gearman_return_t ret= gearman_packet_pack_header(packet);

  if (gearman_failed(ret))
      gearman_packet_free(packet);

  return ret;
}

void gearman_packet_free(gearman_packet_st *packet)
{
  if (packet->args != packet->args_buffer && packet->args != NULL)
  {
    // Created with realloc
    free(packet->args);
    packet->args= NULL;
  }

  if (packet->options.free_data && packet->data != NULL)
  {
    if (packet->universal->workload_free_fn == NULL)
    {
      // Created with malloc()
      free(const_cast<void *>(packet->data)); //@todo fix the need for the casting.
      packet->data= NULL;
    }
    else
    {
      packet->universal->workload_free_fn(const_cast<void *>((packet->data)),
                                          const_cast<void *>(packet->universal->workload_free_context));
    }
  }

  if (! (packet->universal->options.dont_track_packets))
  {
    if (packet->universal->packet_list == packet)
      packet->universal->packet_list= packet->next;
    if (packet->prev != NULL)
      packet->prev->next= packet->next;
    if (packet->next != NULL)
      packet->next->prev= packet->prev;
    packet->universal->packet_count--;
  }

  if (packet->options.allocated)
  {
    delete packet;
  }
}

gearman_return_t gearman_packet_pack_header(gearman_packet_st *packet)
{
  uint64_t length_64;
  uint32_t tmp;

  if (packet->magic == GEARMAN_MAGIC_TEXT)
  {
    packet->options.complete= true;
    return GEARMAN_SUCCESS;
  }

  if (packet->args_size == 0)
  {
    packet->args= packet->args_buffer;
    packet->args_size= GEARMAN_PACKET_HEADER_SIZE;
  }

  switch (packet->magic)
  {
  case GEARMAN_MAGIC_TEXT:
    break;

  case GEARMAN_MAGIC_REQUEST:
    memcpy(packet->args, "\0REQ", 4);
    break;

  case GEARMAN_MAGIC_RESPONSE:
    memcpy(packet->args, "\0RES", 4);
    break;

  default:
    gearman_error(packet->universal, GEARMAN_INVALID_MAGIC, "invalid magic value");
    return GEARMAN_INVALID_MAGIC;
  }

  if (packet->command == GEARMAN_COMMAND_TEXT ||
      packet->command >= GEARMAN_COMMAND_MAX)
  {
    gearman_error(packet->universal, GEARMAN_INVALID_COMMAND, "invalid command value");
    return GEARMAN_INVALID_COMMAND;
  }

  tmp= packet->command;
  tmp= htonl(tmp);
  // Record the command
  memcpy(packet->args + 4, &tmp, 4);

  length_64= packet->args_size + packet->data_size - GEARMAN_PACKET_HEADER_SIZE;

  // Check for overflow on 32bit(portable?).
  if (length_64 >= UINT32_MAX || length_64 < packet->data_size)
  {
    gearman_error(packet->universal, GEARMAN_ARGUMENT_TOO_LARGE, "data size too too long");
    return GEARMAN_ARGUMENT_TOO_LARGE;
  }

  tmp= (uint32_t)length_64;
  tmp= htonl(tmp);
  // Record the length of the packet
  memcpy(packet->args + 8, &tmp, 4);

  packet->options.complete= true;

  return GEARMAN_SUCCESS;
}

gearman_return_t gearman_packet_unpack_header(gearman_packet_st *packet)
{
  uint32_t tmp;

  if (not memcmp(packet->args, "\0REQ", 4))
  {
    packet->magic= GEARMAN_MAGIC_REQUEST;
  }
  else if (!memcmp(packet->args, "\0RES", 4))
  {
    packet->magic= GEARMAN_MAGIC_RESPONSE;
  }
  else
  {
    gearman_error(packet->universal, GEARMAN_INVALID_MAGIC, "invalid magic value");
    return GEARMAN_INVALID_MAGIC;
  }

  memcpy(&tmp, packet->args + 4, 4);
  packet->command= (gearman_command_t)ntohl(tmp);

  if (packet->command == GEARMAN_COMMAND_TEXT ||
      packet->command >= GEARMAN_COMMAND_MAX)
  {
    gearman_error(packet->universal, GEARMAN_INVALID_COMMAND, "invalid command value");
    return GEARMAN_INVALID_COMMAND;
  }

  memcpy(&tmp, packet->args + 8, 4);
  packet->data_size= ntohl(tmp);

  return GEARMAN_SUCCESS;
}

size_t gearman_packet_pack(const gearman_packet_st *packet,
                           void *data, size_t data_size,
                           gearman_return_t *ret_ptr)
{
  if (packet->args_size == 0)
  {
    *ret_ptr= GEARMAN_SUCCESS;
    return 0;
  }

  if (packet->args_size > data_size)
  {
    *ret_ptr= GEARMAN_FLUSH_DATA;
    return 0;
  }

  memcpy(data, packet->args, packet->args_size);
  *ret_ptr= GEARMAN_SUCCESS;
  return packet->args_size;
}

size_t gearman_packet_unpack(gearman_packet_st *packet,
                             const void *data, size_t data_size,
                             gearman_return_t *ret_ptr)
{
  uint8_t *ptr;
  size_t used_size;
  size_t arg_size;

  if (packet->args_size == 0)
  {
    if (data_size > 0 && ((uint8_t *)data)[0] != 0)
    {
      /* Try to parse a text-based command. */
      ptr= (uint8_t *)memchr(data, '\n', data_size);
      if (ptr == NULL)
      {
        *ret_ptr= GEARMAN_IO_WAIT;
        return 0;
      }

      packet->magic= GEARMAN_MAGIC_TEXT;
      packet->command= GEARMAN_COMMAND_TEXT;

      used_size= (size_t)(ptr - ((uint8_t *)data)) + 1;
      *ptr= 0;
      if (used_size > 1 && *(ptr - 1) == '\r')
        *(ptr - 1)= 0;

      for (arg_size= used_size, ptr= (uint8_t *)data; ptr != NULL; data= ptr)
      {
        ptr= (uint8_t *)memchr(data, ' ', arg_size);
        if (ptr != NULL)
        {
          *ptr= 0;
          ptr++;
          while (*ptr == ' ')
            ptr++;

          arg_size-= (size_t)(ptr - ((uint8_t *)data));
        }

        *ret_ptr= packet_create_arg(packet, data, ptr == NULL ? arg_size :
                                    (size_t)(ptr - ((uint8_t *)data)));
        if (*ret_ptr != GEARMAN_SUCCESS)
          return used_size;
      }

      return used_size;
    }
    else if (data_size < GEARMAN_PACKET_HEADER_SIZE)
    {
      *ret_ptr= GEARMAN_IO_WAIT;
      return 0;
    }

    packet->args= packet->args_buffer;
    packet->args_size= GEARMAN_PACKET_HEADER_SIZE;
    memcpy(packet->args, data, GEARMAN_PACKET_HEADER_SIZE);

    *ret_ptr= gearman_packet_unpack_header(packet);
    if (*ret_ptr != GEARMAN_SUCCESS)
      return 0;

    used_size= GEARMAN_PACKET_HEADER_SIZE;
  }
  else
  {
    used_size= 0;
  }

  while (packet->argc != gearman_command_info_list[packet->command].argc)
  {
    if (packet->argc != (gearman_command_info_list[packet->command].argc - 1) ||
        gearman_command_info_list[packet->command].data)
    {
      ptr= (uint8_t *)memchr((char *)data + used_size, 0, data_size - used_size);
      if (ptr == NULL)
      {
        *ret_ptr= GEARMAN_IO_WAIT;
        return used_size;
      }

      arg_size= (size_t)(ptr - ((uint8_t *)data + used_size)) + 1;
      *ret_ptr= packet_create_arg(packet, (uint8_t *)data + used_size, arg_size);

      if (*ret_ptr != GEARMAN_SUCCESS)
        return used_size;

      packet->data_size-= arg_size;
      used_size+= arg_size;
    }
    else
    {
      if ((data_size - used_size) < packet->data_size)
      {
        *ret_ptr= GEARMAN_IO_WAIT;
        return used_size;
      }

      *ret_ptr= packet_create_arg(packet, ((uint8_t *)data) + used_size,
                                  packet->data_size);
      if (*ret_ptr != GEARMAN_SUCCESS)
        return used_size;

      used_size+= packet->data_size;
      packet->data_size= 0;
    }
  }

  *ret_ptr= GEARMAN_SUCCESS;
  return used_size;
}

void gearman_packet_give_data(gearman_packet_st *packet, const void *data,
                              size_t data_size)
{
  packet->data= data;
  packet->data_size= data_size;
  packet->options.free_data= true;
}

void *gearman_packet_take_data(gearman_packet_st *packet, size_t *data_size)
{
  void *data= const_cast<void *>(packet->data);

  *data_size= packet->data_size;

  packet->data= NULL;
  packet->data_size= 0;
  packet->options.free_data= false;

  return data;
}
