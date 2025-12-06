// Auto-generated decision tree model for ESP32
#ifndef FAULT_DETECTION_MODEL_H
#define FAULT_DETECTION_MODEL_H

#pragma once
#include <cstdarg>
namespace Eloquent {
    namespace ML {
        namespace Port {
            class DecisionTree {
                public:
                    /**
                    * Predict class for features vector
                    */
                    int predict(float *x) {
                        if (x[0] <= 1848.0) {
                            if (x[1] <= 0.5) {
                                if (x[0] <= 127.5) {
                                    if (x[0] <= 22.5) {
                                        return 0;
                                    }

                                    else {
                                        return 2;
                                    }
                                }

                                else {
                                    if (x[0] <= 626.0) {
                                        return 0;
                                    }

                                    else {
                                        return 0;
                                    }
                                }
                            }

                            else {
                                if (x[0] <= 891.5) {
                                    if (x[0] <= 787.0) {
                                        return 0;
                                    }

                                    else {
                                        return 0;
                                    }
                                }

                                else {
                                    if (x[0] <= 1008.5) {
                                        return 0;
                                    }

                                    else {
                                        return 0;
                                    }
                                }
                            }
                        }

                        else {
                            if (x[1] <= 0.5) {
                                if (x[0] <= 4071.5) {
                                    if (x[0] <= 1906.5) {
                                        return 2;
                                    }

                                    else {
                                        return 1;
                                    }
                                }

                                else {
                                    return 0;
                                }
                            }

                            else {
                                if (x[0] <= 3780.0) {
                                    return 0;
                                }

                                else {
                                    if (x[0] <= 3820.5) {
                                        return 1;
                                    }

                                    else {
                                        return 0;
                                    }
                                }
                            }
                        }
                    }

                protected:
                };
            }
        }
    }
#endif // FAULT_DETECTION_MODEL_H
