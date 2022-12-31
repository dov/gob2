#include "str.h"

int
main (void)
{
  g_type_init ();

  int the_answer = 42;
  char *stupid_pointer = "ug";

  // This works fine.
  Str *test_good = (Str *) (str_new ("%d", the_answer));
  test_good = test_good;

  // This gets a warning thanks to our function attribute.
  Str *test_bad = (Str *) (str_new ("%d", stupid_pointer));
  test_bad = test_bad;

  return 0;
}
