// TODO: modify to support fma
{
    trivial_op_str_table[OP_subss] = "subss";
    trivial_op_table[OP_subss] = {
        /* Condition lists for each operation type */
        {   /* ConditionList_t for src opnd 0 */
            {   /* Condition_info: is_float, latency, esize, size, check_size */
                {true, 3, 4, 4, 4},
                /* list of Condition_t */
                {   
                }
            },
            /* ConditionList_t for src opnd 1 */
            {   /* Condition_info: is_float, latency, esize, size, check_size */
                {true, 3, 4, 4, 4},
                /* list of Condition_t */
                {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                    {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
                }
            }
        }
    };
}
{
    trivial_op_str_table[OP_subsd] = "subsd";
    trivial_op_table[OP_subsd] = {
        /* Condition lists for each operation type */
        {   /* ConditionList_t for src opnd 0 */
            {   /* Condition_info: is_float, latency, esize, size, check_size */
                {true, 3, 8, 8, 8},
                /* list of Condition_t */
                {   
                }
            },
            /* ConditionList_t for src opnd 1 */
            {   /* Condition_info: is_float, latency, esize, size, check_size */
                {true, 3, 8, 8, 8},
                /* list of Condition_t */
                {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                    {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
                }
            }
        }
    };
}
{
    trivial_op_str_table[OP_subps] = "subps";
    trivial_op_table[OP_subps] = {
        /* Condition lists for each operation type */
        {   /* ConditionList_t for src opnd 0 */
            {   /* Condition_info: is_float, latency, esize, size, check_size */
                {true, 3, 4, 16, 16},
                /* list of Condition_t */
                {   
                }
            },
            /* ConditionList_t for src opnd 1 */
            {   /* Condition_info: is_float, latency, esize, size, check_size */
                {true, 3, 4, 16, 16},
                /* list of Condition_t */
                {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                    {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
                }
            }
        }
    };
}
{
    trivial_op_str_table[OP_subpd] = "subpd";
    trivial_op_table[OP_subpd] = {
        /* Condition lists for each operation type */
        {   /* ConditionList_t for src opnd 0 */
            {   /* Condition_info: is_float, latency, esize, size, check_size */
                {true, 3, 8, 16, 16},
                /* list of Condition_t */
                {   
                }
            },
            /* ConditionList_t for src opnd 1 */
            {   /* Condition_info: is_float, latency, esize, size, check_size */
                {true, 3, 8, 16, 16},
                /* list of Condition_t */
                {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                    {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
                }
            }
        }
    };
}
{
    trivial_op_str_table[OP_vsubss] = "vsubss";
    trivial_op_table[OP_vsubss] = {
        /* Condition lists for each operation type */
        {   /* ConditionList_t for src opnd 0 */
            {   /* Condition_info: is_float, latency, esize, size, check_size */
                {true, 3, 4, 16, 4},
                /* list of Condition_t */
                {   
                }
            },
            /* ConditionList_t for src opnd 1 */
            {   /* Condition_info: is_float, latency, esize, size, check_size */
                {true, 3, 4, 4, 4},
                /* list of Condition_t */
                {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                    {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
                }
            }
        }
    };
}
{
    trivial_op_str_table[OP_vsubsd] = "vsubsd";
    trivial_op_table[OP_vsubsd] = {
        /* Condition lists for each operation type */
        {   /* ConditionList_t for src opnd 0 */
            {   /* Condition_info: is_float, latency, esize, size, check_size */
                {true, 3, 8, 16, 8},
                /* list of Condition_t */
                {   
                }
            },
            /* ConditionList_t for src opnd 1 */
            {   /* Condition_info: is_float, latency, esize, size, check_size */
                {true, 3, 8, 8, 8},
                /* list of Condition_t */
                {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                    {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
                }
            }
        }
    };
}
{
    trivial_op_str_table[OP_vsubps] = "vsubps";
    trivial_op_table[OP_vsubps] = {
        /* Condition lists for each operation type */
        {   /* ConditionList_t for src opnd 0 */
            {   /* Condition_info: is_float, latency, esize, size, check_size */
                {true, 3, 4, 16, 16},
                /* list of Condition_t */
                {   
                }
            },
            /* ConditionList_t for src opnd 1 */
            {   /* Condition_info: is_float, latency, esize, size, check_size */
                {true, 3, 4, 16, 16},
                /* list of Condition_t */
                {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                    {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
                }
            }
        },
        {   /* ConditionList_t for src opnd 0 */
            {   /* Condition_info: is_float, latency, esize, size, check_size */
                {true, 3, 4, 32, 32},
                /* list of Condition_t */
                {   
                }
            },
            /* ConditionList_t for src opnd 1 */
            {   /* Condition_info: is_float, latency, esize, size, check_size */
                {true, 3, 4, 32, 32},
                /* list of Condition_t */
                {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                    {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
                }
            }
        }
    };
}
{
    trivial_op_str_table[OP_vsubpd] = "vsubpd";
    trivial_op_table[OP_vsubpd] = {
        /* Condition lists for each operation type */
        {   /* ConditionList_t for src opnd 0 */
            {   /* Condition_info: is_float, latency, esize, size, check_size */
                {true, 3, 8, 16, 16},
                /* list of Condition_t */
                {   
                }
            },
            /* ConditionList_t for src opnd 1 */
            {   /* Condition_info: is_float, latency, esize, size, check_size */
                {true, 3, 8, 16, 16},
                /* list of Condition_t */
                {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                    {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
                }
            }
        },
        {   /* ConditionList_t for src opnd 0 */
            {   /* Condition_info: is_float, latency, esize, size, check_size */
                {true, 3, 8, 32, 32},
                /* list of Condition_t */
                {   
                }
            },
            /* ConditionList_t for src opnd 1 */
            {   /* Condition_info: is_float, latency, esize, size, check_size */
                {true, 3, 8, 32, 32},
                /* list of Condition_t */
                {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                    {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
                }
            }
        }
    };
}

trivial_op_str_table[OP_vmulss] = "vmulss";
trivial_op_table[OP_vmulss] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 4, 16, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Identical, IS_ONE, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_vmulps] = "vmulps";
trivial_op_table[OP_vmulps] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 4, 16, 16},
            /* list of Condition_t */
            {  /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Identical, IS_ONE, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 4, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    },
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 4, 32, 32},
            /* list of Condition_t */
            {  /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Identical, IS_ONE, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 4, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_vmulsd] = "vmulsd";
trivial_op_table[OP_vmulsd] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 8, 16, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Identical, IS_ONE, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_vmulpd] = "vmulpd";
trivial_op_table[OP_vmulpd] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 8, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Identical, IS_ONE, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 8, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    },
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 8, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Identical, IS_ONE, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 8, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_mulss] = "mulss";
trivial_op_table[OP_mulss] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Identical, IS_ONE, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_mulps] = "mulps";
trivial_op_table[OP_mulps] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 4, 16, 16},
            /* list of Condition_t */
            {  /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Identical, IS_ONE, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 4, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_mulsd] = "mulsd";
trivial_op_table[OP_mulsd] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Identical, IS_ONE, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_mulpd] = "mulpd";
trivial_op_table[OP_mulpd] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 8, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Identical, IS_ONE, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 8, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_addss] = "addss";
trivial_op_table[OP_addss] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_addsd] = "addsd";
trivial_op_table[OP_addsd] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_addps] = "addps";
trivial_op_table[OP_addps] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 4, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 4, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    }
};


trivial_op_str_table[OP_addpd] = "addpd";
trivial_op_table[OP_addpd] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 8, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 8, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_vaddss] = "vaddss";
trivial_op_table[OP_vaddss] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 4, 16, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_vaddsd] = "vaddsd";
trivial_op_table[OP_vaddsd] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 8, 16, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_vaddps] = "vaddps";
trivial_op_table[OP_vaddps] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 4, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 4, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    },
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 4, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 4, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    }
};


trivial_op_str_table[OP_vaddpd] = "vaddpd";
trivial_op_table[OP_vaddpd] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 8, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 8, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    },
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 8, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 3, 8, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_divss] = "divss";
trivial_op_table[OP_divss] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 11, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 11, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Never, IS_ZERO, INVALID, simplify_to_const<ZERO>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_divsd] = "divsd";
trivial_op_table[OP_divsd] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 14, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 14, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Never, IS_ZERO, INVALID, simplify_to_const<ZERO>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_divps] = "divps";
trivial_op_table[OP_divps] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 11, 4, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 11, 4, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Never, IS_ZERO, INVALID, simplify_to_const<ZERO>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_divpd] = "divpd";
trivial_op_table[OP_divpd] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 14, 8, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 14, 8, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Never, IS_ZERO, INVALID, simplify_to_const<ZERO>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_vdivss] = "vdivss";
trivial_op_table[OP_vdivss] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 11, 4, 16, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 11, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Never, IS_ZERO, INVALID, simplify_to_const<ZERO>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_vdivsd] = "vdivsd";
trivial_op_table[OP_vdivsd] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 14, 8, 16, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 14, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Never, IS_ZERO, INVALID, simplify_to_const<ZERO>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_vdivps] = "vdivps";
trivial_op_table[OP_vdivps] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 17, 4, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 17, 4, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Never, IS_ZERO, INVALID, simplify_to_const<ZERO>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_vdivpd] = "vdivpd";
trivial_op_table[OP_vdivpd] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 23, 8, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 23, 8, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Never, IS_ZERO, INVALID, simplify_to_const<ZERO>, 0, 0}
            }
        }
    }
};

/* FMA */
trivial_op_str_table[OP_vfmadd132ps] = "vfmadd132ps";
trivial_op_table[OP_vfmadd132ps] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<1>, 2, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* ->mult, just leave it empty */
            }
        },
        /* ConditionList_t for src opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<1>, 0, 1}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<1>, 2, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* ->mult, just leave it empty */
            }
        },
        /* ConditionList_t for src opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<1>, 0, 1}
            }
        }
    }
};

trivial_op_str_table[OP_vfmadd132pd] = "vfmadd132pd";
trivial_op_table[OP_vfmadd132pd] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<1>, 2, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* ->mult, just leave it empty */
            }
        },
        /* ConditionList_t for src opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<1>, 0, 1}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<1>, 2, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* ->mult, just leave it empty */
            }
        },
        /* ConditionList_t for src opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<1>, 0, 1}
            }
        }
    }
};

trivial_op_str_table[OP_vfmadd213ps] = "vfmadd213ps";
trivial_op_table[OP_vfmadd213ps] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<2>, 1, 2}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<2>, 0, 2}
            }
        },
        /* ConditionList_t for src opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* ->mult, just leave it empty */
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<2>, 1, 2}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<2>, 0, 2}
            }
        },
        /* ConditionList_t for src opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* ->mult, just leave it empty */
            }
        }
    }
};

trivial_op_str_table[OP_vfmadd213pd] = "vfmadd213pd";
trivial_op_table[OP_vfmadd213pd] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<2>, 1, 2}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<2>, 0, 2}
            }
        },
        /* ConditionList_t for src opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* ->mult, just leave it empty */
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<2>, 1, 2}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<2>, 0, 2}
            }
        },
        /* ConditionList_t for src opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* ->mult, just leave it empty */
            }
        }
    }
};

trivial_op_str_table[OP_vfmadd231ps] = "vfmadd231ps";
trivial_op_table[OP_vfmadd231ps] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* ->mult, just leave it empty */
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<0>, 2, 0}
            }
        },
        /* ConditionList_t for src opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<0>, 1, 0}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* ->mult, just leave it empty */
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<0>, 2, 0}
            }
        },
        /* ConditionList_t for src opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<0>, 1, 0}
            }
        }
    }
};

trivial_op_str_table[OP_vfmadd231pd] = "vfmadd231pd";
trivial_op_table[OP_vfmadd231pd] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* ->mult, just leave it empty */
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<0>, 2, 0}
            }
        },
        /* ConditionList_t for src opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<0>, 1, 0}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* ->mult, just leave it empty */
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<0>, 2, 0}
            }
        },
        /* ConditionList_t for src opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<0>, 1, 0}
            }
        }
    }
};

/* scalar FMA */
trivial_op_str_table[OP_vfmadd132ss] = "vfmadd132ss";
trivial_op_table[OP_vfmadd132ss] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<1>, 2, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* ->mult, just leave it empty */
            }
        },
        /* ConditionList_t for src opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<1>, 0, 1}
            }
        }
    }
};

trivial_op_str_table[OP_vfmadd132sd] = "vfmadd132sd";
trivial_op_table[OP_vfmadd132sd] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<1>, 2, 1}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* ->mult, just leave it empty */
            }
        },
        /* ConditionList_t for src opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<1>, 0, 1}
            }
        }
    }
};

trivial_op_str_table[OP_vfmadd213ss] = "vfmadd213ss";
trivial_op_table[OP_vfmadd213ss] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<2>, 1, 2}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<2>, 0, 2}
            }
        },
        /* ConditionList_t for src opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* ->mult, just leave it empty */
            }
        }
    }
};

trivial_op_str_table[OP_vfmadd213sd] = "vfmadd213sd";
trivial_op_table[OP_vfmadd213sd] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<2>, 1, 2}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<2>, 0, 2}
            }
        },
        /* ConditionList_t for src opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* ->mult, just leave it empty */
            }
        }
    }
};

trivial_op_str_table[OP_vfmadd231ss] = "vfmadd231ss";
trivial_op_table[OP_vfmadd231ss] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* ->mult, just leave it empty */
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<0>, 2, 0}
            }
        },
        /* ConditionList_t for src opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<0>, 1, 0}
            }
        }
    }
};

trivial_op_str_table[OP_vfmadd231sd] = "vfmadd231sd";
trivial_op_table[OP_vfmadd231sd] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* ->mult, just leave it empty */
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<0>, 2, 0}
            }
        },
        /* ConditionList_t for src opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, OTHER, simplify_to_mov<0>, 1, 0}
            }
        }
    }
};

trivial_op_str_table[OP_rcpss] = "rcpss";
trivial_op_table[OP_rcpss] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Never, IS_ZERO, INVALID, simplify_to_const<ZERO>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_rcpps] = "rcpps";
trivial_op_table[OP_rcpps] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Never, IS_ZERO, INVALID, simplify_to_const<ZERO>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_vrcpps] = "vrcpps";
trivial_op_table[OP_vrcpps] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 7, 4, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Never, IS_ZERO, INVALID, simplify_to_const<ZERO>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_roundss] = "roundss";
trivial_op_table[OP_roundss] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 6, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Function, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 6, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* empty */
            }
        }
    }
};

trivial_op_str_table[OP_roundsd] = "roundsd";
trivial_op_table[OP_roundsd] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 6, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Function, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 6, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* empty */
            }
        }
    }
};

trivial_op_str_table[OP_roundps] = "roundps";
trivial_op_table[OP_roundps] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 6, 4, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Function, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 6, 4, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* empty */
            }
        }
    }
};

trivial_op_str_table[OP_roundpd] = "roundpd";
trivial_op_table[OP_roundpd] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 6, 8, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Function, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0}
            }
        },
        /* ConditionList_t for src opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 6, 8, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* empty */
            }
        }
    }
};

trivial_op_str_table[OP_sqrtss] = "sqrtss";
trivial_op_table[OP_sqrtss] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 11, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Function, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Function, IS_ONE, ONE, simplify_to_const<ONE>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_sqrtsd] = "sqrtsd";
trivial_op_table[OP_sqrtsd] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 16, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Function, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Function, IS_ONE, ONE, simplify_to_const<ONE>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_sqrtps] = "sqrtps";
trivial_op_table[OP_sqrtps] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 11, 4, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Function, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Function, IS_ONE, ONE, simplify_to_const<ONE>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_sqrtpd] = "sqrtpd";
trivial_op_table[OP_sqrtpd] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 16, 8, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Function, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Function, IS_ONE, ONE, simplify_to_const<ONE>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_vsqrtps] = "vsqrtps";
trivial_op_table[OP_vsqrtps] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 19, 4, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Function, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Function, IS_ONE, ONE, simplify_to_const<ONE>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_vsqrtpd] = "vsqrtpd";
trivial_op_table[OP_vsqrtpd] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 29, 8, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Function, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Function, IS_ONE, ONE, simplify_to_const<ONE>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_rsqrtss] = "rsqrtss";
trivial_op_table[OP_rsqrtss] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Never, IS_ZERO, INVALID, simplify_to_const<ZERO>, 0, 0},
                {Function, IS_ONE, ONE, simplify_to_const<ONE>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_rsqrtps] = "rsqrtps";
trivial_op_table[OP_rsqrtps] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 5, 4, 16, 16},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Never, IS_ZERO, INVALID, simplify_to_const<ZERO>, 0, 0},
                {Function, IS_ONE, ONE, simplify_to_const<ONE>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_vrsqrtps] = "vrsqrtps";
trivial_op_table[OP_vrsqrtps] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for src opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {true, 7, 4, 32, 32},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Never, IS_ZERO, INVALID, simplify_to_const<ZERO>, 0, 0},
                {Function, IS_ONE, ONE, simplify_to_const<ONE>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_add] = "add";
trivial_op_table[OP_add] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 2, 2, 2},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 2, 2, 2},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_mul] = "mul";
trivial_op_table[OP_mul] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 3, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Identical, IS_ONE, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 3, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 4, 2, 2, 2},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Identical, IS_ONE, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 4, 2, 2, 2},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 4, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Identical, IS_ONE, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 4, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 3, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Identical, IS_ONE, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 3, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_imul] = "imul";
trivial_op_table[OP_imul] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 3, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Identical, IS_ONE, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 3, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 4, 2, 2, 2},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Identical, IS_ONE, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 4, 2, 2, 2},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 4, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Identical, IS_ONE, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 4, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 3, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Identical, IS_ONE, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 3, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_mulx] = "mulx";
trivial_op_table[OP_mulx] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 4, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Identical, IS_ONE, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 4, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 3, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Identical, IS_ONE, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 3, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_ONE, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_div] = "div";
trivial_op_table[OP_div] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 25, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Never, IS_ZERO, INVALID, simplify_to_const<ZERO>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 25, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* empty */
            }
        },
        /* ConditionList_t for opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 25, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* empty */
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 26, 2, 2, 2},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Never, IS_ZERO, INVALID, simplify_to_const<ZERO>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 26, 2, 2, 2},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* empty */
            }
        },
        /* ConditionList_t for opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 26, 2, 2, 2},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* empty */
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 29, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Never, IS_ZERO, INVALID, simplify_to_const<ZERO>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 29, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* empty */
            }
        },
        /* ConditionList_t for opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 29, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* empty */
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 95, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Never, IS_ZERO, INVALID, simplify_to_const<ZERO>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 95, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* empty */
            }
        },
        /* ConditionList_t for opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 95, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* empty */
            }
        }
    }
};

trivial_op_str_table[OP_idiv] = "idiv";
trivial_op_table[OP_idiv] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 26, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Never, IS_ZERO, INVALID, simplify_to_const<ZERO>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 26, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* empty */
            }
        },
        /* ConditionList_t for opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 26, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* empty */
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 26, 2, 2, 2},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Never, IS_ZERO, INVALID, simplify_to_const<ZERO>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 26, 2, 2, 2},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* empty */
            }
        },
        /* ConditionList_t for opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 26, 2, 2, 2},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* empty */
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 29, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Never, IS_ZERO, INVALID, simplify_to_const<ZERO>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 29, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* empty */
            }
        },
        /* ConditionList_t for opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 29, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* empty */
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 103, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Never, IS_ZERO, INVALID, simplify_to_const<ZERO>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 103, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* empty */
            }
        },
        /* ConditionList_t for opnd 2 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 103, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                /* empty */
            }
        }
    }
};

trivial_op_str_table[OP_and] = "and";
trivial_op_table[OP_and] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Identical, IS_FULL, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_FULL, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 2, 2, 2},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Identical, IS_FULL, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 2, 2, 2},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_FULL, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Identical, IS_FULL, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_FULL, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Identical, IS_FULL, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_FULL, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_or] = "or";
trivial_op_table[OP_or] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 1, 1},
                {Identical, IS_ZERO, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 0, 0},
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 2, 2, 2},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 1, 1},
                {Identical, IS_ZERO, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 2, 2, 2},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 0, 0},
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 1, 1},
                {Identical, IS_ZERO, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 0, 0},
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 1, 1},
                {Identical, IS_ZERO, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 0, 0},
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    }
};

trivial_op_str_table[OP_shr] = "shr";
trivial_op_table[OP_shr] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 2, 2, 2},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1}
            }
        }
    }
};

trivial_op_str_table[OP_shl] = "shl";
trivial_op_table[OP_shl] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 2, 2, 2},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1}
            }
        }
    }
};

trivial_op_str_table[OP_sar] = "sar";
trivial_op_table[OP_sar] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 1, 1}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 2, 2, 2},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 1, 1}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 1, 1}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 1, 1}
            }
        }
    }
};

trivial_op_str_table[OP_ror] = "ror";
trivial_op_table[OP_ror] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 1, 1}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 2, 2, 2},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 1, 1}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 1, 1}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 1, 1}
            }
        }
    }
};

trivial_op_str_table[OP_rol] = "rol";
trivial_op_table[OP_rol] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 1, 1}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 2, 2, 2},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 1, 1}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 1, 1}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 1, 1}
            }
        }
    }
};

trivial_op_str_table[OP_rcr] = "rcr";
trivial_op_table[OP_rcr] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 1, 1}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 2, 2, 2},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 1, 1}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 1, 1}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 1, 1}
            }
        }
    }
};

trivial_op_str_table[OP_rcl] = "rcl";
trivial_op_table[OP_rcl] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 1, 1}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 2, 2, 2},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 1, 1}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 1, 1}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 1, 1, 1},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0, 0}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 2, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Absorbing, IS_FULL, FULL, simplify_to_const<FULL>, 1, 1}
            }
        }
    }
};


trivial_op_str_table[OP_andn] = "andn";
trivial_op_table[OP_andn] = {
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Absorbing, IS_FULL, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 4, 4, 4},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_FULL, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    },
    /* Condition lists for each operation type */
    {   /* ConditionList_t for opnd 0 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Identical, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1, 1},
                {Absorbing, IS_FULL, OTHER, simplify_to_mov<1>, 1, 1}
            }
        },
        /* ConditionList_t for opnd 1 */
        {   /* Condition_info: is_float, latency, esize, size, check_size */
            {false, 1, 8, 8, 8},
            /* list of Condition_t */
            {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
                {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 0, 0},
                {Identical, IS_FULL, OTHER, simplify_to_mov<0>, 0, 0}
            }
        }
    }
};

// trivial_op_str_table[OP_fscale] = "fscale";
// trivial_op_table[OP_fscale] = {
//     /* Condition lists for each operation type */
//     {   /* ConditionList_t for opnd 0 */
//         {   /* Condition_info: is_float, latency, esize, size, check_size */
//             {true, 125, 4, 4},
//             /* list of Condition_t */
//             {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
//                 {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1}
//             }
//         },
//         /* ConditionList_t for opnd 1 */
//         {   /* Condition_info: is_float, latency, esize, size, check_size */
//             {true, 125, 4, 4},
//             /* list of Condition_t */
//             {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
//                 {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0}
//             }
//         }
//     },
//     /* Condition lists for each operation type */
//     {   /* ConditionList_t for opnd 0 */
//         {   /* Condition_info: is_float, latency, esize, size, check_size */
//             {true, 125, 8, 8},
//             /* list of Condition_t */
//             {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
//                 {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1}
//             }
//         },
//         /* ConditionList_t for opnd 1 */
//         {   /* Condition_info: is_float, latency, esize, size, check_size */
//             {true, 125, 8, 8},
//             /* list of Condition_t */
//             {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
//                 {Identical, IS_ZERO, OTHER, simplify_to_mov<0>, 0}
//             }
//         }
//     }
// };

// trivial_op_str_table[OP_fsqrt] = "fsqrt";
// trivial_op_table[OP_fscale] = {
//     /* Condition lists for each operation type */
//     {   /* ConditionList_t for opnd 0 */
//         {   /* Condition_info: is_float, latency, esize, size, check_size */
//             {true, 23, 4, 4},
//             /* list of Condition_t */
//             {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
//                 {Function, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1},
//                 {Function, IS_ONE, ONE, simplify_to_const<ONE>, 1}
//             }
//         }
//     },
//     /* Condition lists for each operation type */
//     {   /* ConditionList_t for opnd 0 */
//         {   /* Condition_info: is_float, latency, esize, size, check_size */
//             {true, 23, 8, 8},
//             /* list of Condition_t */
//             {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
//                 {Function, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1},
//                 {Function, IS_ONE, ONE, simplify_to_const<ONE>, 1}
//             }
//         }
//     }
// };

// trivial_op_str_table[OP_fsin] = "fsin";
// trivial_op_table[OP_fsin] = {
//     /* Condition lists for each operation type */
//     {   /* ConditionList_t for opnd 0 */
//         {   /* Condition_info: is_float, latency, esize, size, check_size */
//             {true, 106, 4, 4},
//             /* list of Condition_t */
//             {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
//                 {Function, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1}
//             }
//         }
//     },
//     /* Condition lists for each operation type */
//     {   /* ConditionList_t for opnd 0 */
//         {   /* Condition_info: is_float, latency, esize, size, check_size */
//             {true, 106, 8, 8},
//             /* list of Condition_t */
//             {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
//                 {Function, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1}
//             }
//         }
//     }
// };

// trivial_op_str_table[OP_fcos] = "fcos";
// trivial_op_table[OP_fcos] = {
//     /* Condition lists for each operation type */
//     {   /* ConditionList_t for opnd 0 */
//         {   /* Condition_info: is_float, latency, esize, size, check_size */
//             {true, 112, 4, 4},
//             /* list of Condition_t */
//             {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
//                 {Function, IS_ZERO, ONE, simplify_to_const<ONE>, 1}
//             }
//         }
//     },
//     /* Condition lists for each operation type */
//     {   /* ConditionList_t for opnd 0 */
//         {   /* Condition_info: is_float, latency, esize, size, check_size */
//             {true, 112, 8, 8},
//             /* list of Condition_t */
//             {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
//                 {Function, IS_ZERO, ONE, simplify_to_const<ONE>, 1}
//             }
//         }
//     }
// };

// trivial_op_str_table[OP_f2xm1] = "f2xm1";
// trivial_op_table[OP_f2xm1] = {
//     /* Condition lists for each operation type */
//     {   /* ConditionList_t for opnd 0 */
//         {   /* Condition_info: is_float, latency, esize, size, check_size */
//             {true, 68, 4, 4},
//             /* list of Condition_t */
//             {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
//                 {Function, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1},
//                 {Function, IS_ONE, ONE, simplify_to_const<ONE>, 1}
//             }
//         }
//     },
//     /* Condition lists for each operation type */
//     {   /* ConditionList_t for opnd 0 */
//         {   /* Condition_info: is_float, latency, esize, size, check_size */
//             {true, 68, 8, 8},
//             /* list of Condition_t */
//             {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
//                 {Function, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1},
//                 {Function, IS_ONE, ONE, simplify_to_const<ONE>, 1}
//             }
//         }
//     }
// };

// trivial_op_str_table[OP_fyl2x] = "fyl2x";
// trivial_op_table[OP_fyl2x] = {
//     /* Condition lists for each operation type */
//     {   /* ConditionList_t for opnd 0 */
//         {   /* Condition_info: is_float, latency, esize, size, check_size */
//             {true, 92, 4, 4},
//             /* list of Condition_t */
//             {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
//                 {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1}
//             }
//         },
//         /* ConditionList_t for opnd 1 */
//         {   /* Condition_info: is_float, latency, esize, size, check_size */
//             {true, 92, 4, 4},
//             /* list of Condition_t */
//             {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
//                 {Absorbing, IS_ONE, ZERO, simplify_to_const<ZERO>, 1}
//             }
//         }
//     },
//     /* Condition lists for each operation type */
//     {   /* ConditionList_t for opnd 0 */
//         {   /* Condition_info: is_float, latency, esize, size, check_size */
//             {true, 92, 8, 8},
//             /* list of Condition_t */
//             {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
//                 {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1}
//             }
//         },
//         /* ConditionList_t for opnd 1 */
//         {   /* Condition_info: is_float, latency, esize, size, check_size */
//             {true, 92, 8, 8},
//             /* list of Condition_t */
//             {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
//                 {Absorbing, IS_ONE, ZERO, simplify_to_const<ZERO>, 1}
//             }
//         }
//     }
// };

// trivial_op_str_table[OP_fyl2xp1] = "fyl2xp1";
// trivial_op_table[OP_fyl2xp1] = {
//     /* Condition lists for each operation type */
//     {   /* ConditionList_t for opnd 0 */
//         {   /* Condition_info: is_float, latency, esize, size, check_size */
//             {true, 74, 4, 4},
//             /* list of Condition_t */
//             {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
//                 {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1}
//             }
//         },
//         /* ConditionList_t for opnd 1 */
//         {   /* Condition_info: is_float, latency, esize, size, check_size */
//             {true, 74, 4, 4},
//             /* list of Condition_t */
//             {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
//                 {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1}
//             }
//         }
//     },
//     /* Condition lists for each operation type */
//     {   /* ConditionList_t for opnd 0 */
//         {   /* Condition_info: is_float, latency, esize, size, check_size */
//             {true, 74, 8, 8},
//             /* list of Condition_t */
//             {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
//                 {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1}
//             }
//         },
//         /* ConditionList_t for opnd 1 */
//         {   /* Condition_info: is_float, latency, esize, size, check_size */
//             {true, 74, 8, 8},
//             /* list of Condition_t */
//             {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
//                 {Absorbing, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1}
//             }
//         }
//     }
// };

// trivial_op_str_table[OP_fptan] = "fptan";
// trivial_op_table[OP_fptan] = {
//     /* Condition lists for each operation type */
//     {   /* ConditionList_t for opnd 0 */
//         {   /* Condition_info: is_float, latency, esize, size, check_size */
//             {true, 132, 4, 4},
//             /* list of Condition_t */
//             {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
//                 {Function, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1}
//             }
//         }
//     },
//     /* Condition lists for each operation type */
//     {   /* ConditionList_t for opnd 0 */
//         {   /* Condition_info: is_float, latency, esize, size, check_size */
//             {true, 132, 8, 8},
//             /* list of Condition_t */
//             {   /* Condition_t: attr, val, res, simplify, backward_idx, other_idx */
//                 {Function, IS_ZERO, ZERO, simplify_to_const<ZERO>, 1}
//             }
//         }
//     }
// };