#include <config.h>
#include <libvisual_cpp.hpp>
#include <libvisual/libvisual.h>
#include <iostream>
#include <cstdlib>
#include <cstring>

namespace
{
  bool trap_exceptions = false;
  
  void (*last_terminate_handler) () = 0;
  void (*last_unexpected_handler) () = 0;

  void handle_unexpected ()
  {
      std::cerr << "Unexpected exception caught" << std::endl;
      throw Lv::unexpected_error ();
  }

  void handle_terminate ()
  {
      std::cerr << "Uncaught exception, aborting program" << std::endl;
      std::abort ();
  }

  void set_exception_handlers ()
  {
      last_unexpected_handler = std::set_unexpected (handle_unexpected);
      last_terminate_handler = std::set_terminate (handle_terminate);
  }

  void unset_exception_handlers ()
  {
      std::set_unexpected (last_unexpected_handler);
      last_unexpected_handler = 0;

      std::set_terminate (last_terminate_handler);
      last_terminate_handler = 0;
  }

  void init_common (bool trap_exceptions_)
  {
      trap_exceptions = trap_exceptions_;
      if (trap_exceptions)
          set_exception_handlers ();
  }
}

namespace Lv
{
  const char *get_lv_version ()
  {
      return visual_get_version ();
  }

  int init (int& argc, char **& argv, bool trap_exceptions_)
  {
      int result = visual_init (&argc, &argv);

      if (result == VISUAL_OK)
          init_common (trap_exceptions_);

      return result;
  }

  int init (bool trap_exceptions_)
  {
      int result = visual_init (NULL, NULL);

      if (result == VISUAL_OK)
          init_common (trap_exceptions_);

      return result;      
  }

  bool is_init ()
  {
      return visual_is_initialized ();
  }

  int quit ()
  {
      if (is_init () && trap_exceptions)
          unset_exception_handlers ();

      return visual_quit ();
  }

  int init_path_add (const std::string& path)
  {
      // visual_init_path_add () currently does not duplicate the path
      // string (CVS 20050901). Passing path.c_str () directly is an
      // error because the actual parameter of path may go out of
      // scope (esp for temp. objects). Consider:
      //   Lv::init_path_add (prefix + std::string("/dir"));

      std::size_t length = std::strlen (path.c_str ()) + 1;

      char *str = new char[length];
      std::memcpy (str, path.c_str (), length);

      return visual_init_path_add (str);
  }

} // namespace Lv


#ifdef LIBVISUAL_CPP_TEST

void throw_unexpected_error ()
    throw ()
{
    throw std::runtime_error ("throw unexpected exception");
}

int main (int argc, char **argv)
{
    std::cout << "Libvisual version: " << Lv::get_lv_version () << "\n";

    // startup test
    std::cout << "Startup test\n";

    try
    {
        if (Lv::init (argc, argv) != VISUAL_OK)
            throw std::runtime_error ("Lv::init (argc, argv) fail");

        if (!Lv::is_init ())
            throw std::runtime_error ("Lv::is_init () returns false");

        if (Lv::quit () != VISUAL_OK)
            throw std::runtime_error ("Lv::quit () fail");

        if (Lv::init () != VISUAL_OK)
            throw std::runtime_error ("Lv::init (NULL, NULL) fail");

        if (!Lv::is_init ())
            throw std::runtime_error ("Lv::is_init () returns false");
    }
    catch (std::runtime_error& error)
    {
        std::cout << error.what () << std::endl;
    }

    // uncaught exception test

    try
    {
        std::cout << "Unexpected exception test\n";
        throw_unexpected_error ();
    }
    catch (Lv::unexpected_error& error)
    {
        std::cout << "unexpected_error caught\n";
        std::cout << error.what () << std::endl;
    }

    std::cout << "Uncaught exception test\n";
    throw std::runtime_error ("test uncaught exception");
}

#endif // #ifdef LIBVISUAL_CPP_TEST