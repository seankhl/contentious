1. tail optimization
2. mut\_set for appropriate ones (tr\_vector) X
3. pop\_back
4. lock-free detach/reattach/resolution procedure !
5. par/log-savvy resolution procedure P
6. use intrusive pointers and variants to save space X
7. ownership problem !
8. use coroutines to do auto parallel load balancing P
9. use impl headers instead of cc files X
10. clean up or finish the raw versions ?

11. use facebook synchronized queue for threadpool
12. only one resolving queue
13. let any thread do resolution, but make sure they are correctly ordered
14. shallow copy should walk up and down the depth
15. efficient deep copy (leaves)
16. finish bp_vector_base members, including constructors
17. fix hackish members
18. fancy kernel calling syntactic sugar
