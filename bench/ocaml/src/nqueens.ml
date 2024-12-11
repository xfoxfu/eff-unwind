(* cannot implement since it requires multishot *)

open Effect
open Effect.Deep

type _ Effect.t += Pick : int -> int t
exception Fail

let rec safe queen diag xs =
  match xs with
  | [] -> true
  | q :: qs -> if (queen <> q && queen <> q + diag && queen <> q - diag)
    then safe queen (diag + 1) qs
    else false

let rec place size column : int list =
  if column = 0
    then []
    else begin
      let rest = place size (column - 1) in
      let next = perform (Pick size) in
      if safe next 1 rest
        then next :: rest
        else raise Fail
    end

let run n =
  match_with (place n) n
  { retc = (fun _x -> 1);
    exnc = (fun _ -> 0);
    effc = fun (type a) (eff: a t) ->
      match eff with
      | Pick size -> Some (fun (cont: (a, _) continuation) ->
          let rec loop i a =
            if i = size then (a + continue cont i)
            else loop (i+1) (a + continue (Obj.clone_continuation cont) i)
          in loop 1 0)
      | _ -> None }

let main () =
  let n = try int_of_string Sys.argv.(1) with _ -> 5 in
  let r = run n in
  Printf.printf "%d\n" r

let _ = main()

