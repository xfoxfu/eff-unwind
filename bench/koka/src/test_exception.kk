module main

import std/os/env

effect exception
  fun exception(): int

fun foobar(): <exception> int
  exception()
  56

fun has-handler(): int
  with handler
    fun exception() 72
  foobar()

fun do(n : int)
  if n > 0 then {
    val i = has-handler()
    do ( n - 1 )
  }

pub fun main()
  val n = 1000000
  do(n)
