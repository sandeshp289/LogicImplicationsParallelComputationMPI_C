# LogicImplicationsParallelComputationMPI_C

Description:

    Parallel Computation of Static Logic Implications (Direct, Indirect, Contrapositive) using MPI and C

    implications_parallel.c                       : C/C++ code


Notes: 

      implications_sequential.c computes the static logic implications sequentially. 

          This is used to compare the sequential computation run time against the parallel computation run time

      Experimental determination of Tp (parallel run time) and Ts (sequential run time):
      
          Tp is determined by measuring the difference between the start time just before calling master
      
          and the end time just after the master returns. In this case there will be multiple slave processors
      
          involved.
      
          Ts is determined by measuring the difference between the start time just before calling master
      
          and the end time just after the master returns. In this case there will be only one processor and
      
          the master runs on that sequentially.

          The experiments were performed for various ISCAS85 benchmark combinational and sequential circuits.
