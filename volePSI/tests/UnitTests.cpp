

#include "cryptoTools/Common/Log.h"
#include <functional>
#include "UnitTests.h"

#include "Paxos_Tests.h"
#include "FileBase_Tests.h"

namespace volePSI_Tests
{
    oc::TestCollection Tests([](oc::TestCollection& t) {
        
        t.add("Paxos_buildRow_Test         ", Paxos_buildRow_Test);
        t.add("Paxos_solve_Test            ", Paxos_solve_Test);
        t.add("Paxos_solve_u8_Test         ", Paxos_solve_u8_Test);
        t.add("Paxos_solve_mtx_Test        ", Paxos_solve_mtx_Test);
                                           
        t.add("Paxos_invE_Test             ", Paxos_invE_Test);
        t.add("Paxos_invE_g3_Test          ", Paxos_invE_g3_Test);
        t.add("Paxos_solve_gap_Test        ", Paxos_solve_gap_Test);
        t.add("Paxos_solve_rand_Test       ", Paxos_solve_rand_Test);
        t.add("Paxos_solve_rand_gap_Test   ", Paxos_solve_rand_gap_Test);
                                           
        t.add("Baxos_solve_Test            ", Baxos_solve_Test);
        t.add("Baxos_solve_mtx_Test        ", Baxos_solve_mtx_Test);
        t.add("Baxos_solve_par_Test        ", Baxos_solve_par_Test);
        t.add("Baxos_solve_rand_Test       ", Baxos_solve_rand_Test);
        
        t.add("filebase_readSet_Test       ", filebase_readSet_Test);
        t.add("filebase_psi_bin_Test       ", filebase_psi_bin_Test);
        t.add("filebase_psi_csv_Test       ", filebase_psi_csv_Test);
        t.add("filebase_psi_csvh_Test      ", filebase_psi_csvh_Test);

    });
}
