
#ifndef BP_CONT_VECTOR_TESTS_H
#define BP_CONT_VECTOR_TESTS_H

#include <vector>

double cont_reduce_manual(const std::vector<double> &test_vec);
//double cont_foreach_manual(const std::vector<double> &test_vec);

void cont_reduce(const std::vector<double> &test_vec);
void cont_foreach(const std::vector<double> &test_vec);
void cont_stencil(const std::vector<double> &test_vec);

void cont_testing();

#endif // BP_CONT_VECTOR_TESTS_H

