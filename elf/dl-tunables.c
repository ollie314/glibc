/* The tunable framework.  See the README to know how to use the tunable in
   a glibc module.

   Copyright (C) 2016 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.  */

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>

#define TUNABLES_INTERNAL 1
#include "dl-tunables.h"

/* Compare environment names, bounded by the name hardcoded in glibc.  */
static bool
is_name (const char *orig, const char *envname)
{
  for (;*orig != '\0' && *envname != '\0'; envname++, orig++)
    if (*orig != *envname)
      break;

  /* The ENVNAME is immediately followed by a value.  */
  if (*orig == '\0' && *envname == '=')
    return true;
  else
    return false;
}

static bool
get_next_env (char ***envp, char **name, size_t *namelen, char **val)
{
  char **ev = *envp;

  while (ev != NULL && *ev != '\0')
    {
      char *envline = *ev;
      int len = 0;

      while (envline[len] != '\0' && envline[len] != '=')
	len++;

      /* Just the name and no value, go to the next one.  */
      if (envline[len] == '\0')
	continue;

      *name = envline;
      *namelen = len;
      *val = &envline[len + 1];
      *envp = ++ev;

      return true;
    }

  return false;
}


#define RETURN_IF_INVALID_RANGE(__cur, __val) \
({									      \
  __typeof ((__cur)->type.min) min = (__cur)->type.min;			      \
  __typeof ((__cur)->type.max) max = (__cur)->type.max;			      \
  if (min != max && ((__val) < min || (__val) > max))			      \
    return;								      \
})

/* Validate range of the input value and initialize the tunable CUR if it looks
   good.  */
static void
tunable_initialize (tunable_t *cur)
{
  switch (cur->type.type_code)
    {
    case TUNABLE_TYPE_INT_32:
	{
	  int32_t val = (int32_t) __strtoul_internal (cur->strval, NULL, 0, 0);
	  RETURN_IF_INVALID_RANGE (cur, val);
	  cur->val.numval = (int64_t) val;
	  break;
	}
    case TUNABLE_TYPE_SIZE_T:
	{
	  size_t val = (size_t) __strtoul_internal (cur->strval, NULL, 0, 0);
	  RETURN_IF_INVALID_RANGE (cur, val);
	  cur->val.numval = (int64_t) val;
	  break;
	}
    case TUNABLE_TYPE_STRING:
	{
	  cur->val.strval = cur->strval;
	  break;
	}
    default:
      __builtin_unreachable ();
    }
}

/* Initialize the tunables list from the environment.  For now we only use the
   ENV_ALIAS to find values.  Later we will also use the tunable names to find
   values.  */
void
__tunables_init (char **envp)
{
  char *envname = NULL;
  char *envval = NULL;
  size_t len = 0;

  while (get_next_env (&envp, &envname, &len, &envval))
    {
      for (int i = 0; i < sizeof (tunable_list) / sizeof (tunable_t); i++)
	{
	  tunable_t *cur = &tunable_list[i];

	  /* Skip over tunables that have either been set already or should be
	     skipped.  */
	  if (cur->strval != NULL || cur->env_alias == NULL
	      || (__libc_enable_secure && !cur->secure_env_alias))
	    continue;

	  const char *name = cur->env_alias;

	  /* We have a match.  Initialize and move on to the next line.  */
	  if (is_name (name, envname))
	    {
	      cur->strval = envval;
	      tunable_initialize (cur);
	      break;
	    }
	}
    }
}

/* Set the tunable value.  This is called by the module that the tunable exists
   in. */
void
__tunable_set_val (tunable_id_t id, void *valp, tunable_callback_t callback)
{
  tunable_t *cur = &tunable_list[id];

  /* Sanity check: don't do anything if our tunable was not set during
     initialization.  */
  if (cur->strval == NULL)
    return;

  switch (cur->type.type_code)
    {
    case TUNABLE_TYPE_INT_32:
	{
	  *((int32_t *) valp) = (int32_t) cur->val.numval;
	  break;
	}
    case TUNABLE_TYPE_SIZE_T:
	{
	  *((size_t *) valp) = (size_t) cur->val.numval;
	  break;
	}
    case TUNABLE_TYPE_STRING:
	{
	  *((const char **)valp) = cur->strval;
	  break;
	}
    default:
      __builtin_unreachable ();
    }

  if (callback)
    callback (&cur->val);
}
