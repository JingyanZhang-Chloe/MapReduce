Current plan: theft is done by workers themselves

Design of threads:

- Master thread
    - Input: `num_workers`, `seeds` (vector), `successors` (function returning a vector), `map_func`, `reduce_func`
    - Initiate a binary tree of `known` nodes with `seeds`
        - For now, reference to a binary tree, if time allows, try fine-grained (to allow workers to check `known` concurrently faster)
    - Create a list of workers and start the workers
    - Maintain the number of active workers
    - Receive the partial results from workers
        - decrement the number of active workers
    - Perform final map-reduce on partial results
    - Shut down
        - Ensure it is triggered once
        - Can be manual (abort)
        - Automatic if `active_workers` is 0 OR there is an error (hopefully not)
    - (maybe) Theft:
        - Receives theft requests (when a worker runs out of tasks)
            - Queue of requests
        - Chooses a victim (somehow already knows whom to choose)
            - Tells the thief the ID of the victim
- Worker thread
    - Input: worker ID, initial vector of nodes to process
    - Initiate a partial result for `reduce`
    - Maintain a queue of nodes (created from the given initial nodes)
    - Take a node (if not possible, then steal) for computation
    - Computation (`map_reduce(node)`):
        - Do `map`
        - Do `reduce(curr_res, mapped_val)`
        - Compute successors that are not in `known`
            - (if possible) Take a successor and do `map_reduce(node)`
            - Add the rest of the successors to the queue and to `known` (with lock?)
    - (if not done by the master) Steal:
        - Take the next worker ID (`(my_id + 1) % n`)
        - Lock the victim’s queue
        - Take a node (and remove it from the queue) if possible
        - Unlock the victim’s queue
        - Do `map_reduce(node)` if a node was acquired
        - Otherwise, go to the next worker ID
            - If reached our ID (i.e., no one to steal from), send the partial result to the master
    - (maybe) In case of error, send a signal to the master

Possible optimization:

- Better theft mechanism
- Fine-grained locking of `known` to allow multiple workers to use `add` and `contains` when computing successors at the same time
