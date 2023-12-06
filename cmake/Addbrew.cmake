find_program(HOMEBREW_BIN brew HINTS /opt/homebrew/bin/brew)
if (HOMEBREW_BIN)
  message(STATUS "Homebrew found: ${HOMEBREW_BIN}")
  
  function(add_brew_prefix NAME)
    execute_process(COMMAND ${HOMEBREW_BIN} --prefix ${NAME}
      OUTPUT_VARIABLE ${NAME}_PREFIX
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    message(STATUS "Found Homebrew ${NAME}: ${${NAME}_PREFIX}")
    set(BREW_${NAME}_PREFIX ${${NAME}_PREFIX} PARENT_SCOPE)
  endfunction()
endif ()

mark_as_advanced(HOMEBREW_BIN)
