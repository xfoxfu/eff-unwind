(* cannot implement since it requires multishot *)

open Effect
open Effect.Deep

type _ Effect.t += Choose : unit -> bool t

type tree =
| Leaf
| Node of tree * int * tree

let operator x y = abs (x - (503 * y) + 37) mod 1009

let rec make = function
  | 0 -> Leaf
  | n -> let t = make (n-1) in Node (t,n,t)

let run n =
  let tree = make n in
  let state = ref 0 in

  let rec explore t =
    match t with
    | Leaf -> !state
    | Node (l, v, r) ->
      let next = if (perform (Choose ())) then l else r in
      state := operator !state v;
      operator v (explore next) in

  let paths () =
    match_with explore tree
    { retc = (fun _x -> [_x]);
      exnc = raise;
      effc = fun (type a) (eff: a t) ->
        match eff with
        | Choose () -> Some (fun (cont: (a, _) continuation) ->
            let l = continue (Obj.clone_continuation cont) true in
            let r = continue cont false in
            l @ r)
        | _ -> None } in
  let rec loop i =
    if i = 0
      then !state
      else (
        state := List.fold_left max 0 (paths ());
        loop (i - 1)
      ) in

  loop 10

let main () =
  let n = try int_of_string (Sys.argv.(1)) with _ -> 5 in
  let r = run n in
  Printf.printf "%d\n" r

let _ = main ()
