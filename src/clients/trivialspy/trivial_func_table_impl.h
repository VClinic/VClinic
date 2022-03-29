#define _TRIVIAL_EXP 0
#define _TRIVIAL_EXPF 1
#define _TRIVIAL_SIN 2
#define _TRIVIAL_SINF 3
#define _TRIVIAL_COS 4
#define _TRIVIAL_COSF 5
#define _TRIVIAL_LOG 6
#define _TRIVIAL_LOGF 7
#define _TRIVIAL_POW 8
#define _TRIVIAL_POWF 9
static int lookup(std::string func_name) {
    size_t pos_start = 0;
    size_t pos_end = func_name.find('@', pos_start);
    DPRINTF("lookup: func_name=%s\n", func_name.c_str());
    if(pos_end!=std::string::npos) {
        std::string func_base_name = func_name.substr(pos_start, pos_end - pos_start);
        DPRINTF("func_name=%s, base_name=%s\n", func_name.c_str(), func_base_name.c_str());
        // for(auto it=trivial_func_list.begin(); it!=trivial_func_list.end(); ++it) {
        //     if(func_base_name==(*it).name) {
        //         return (*it).entry;
        //     }
        // }
    }
    return -1;
}

static void init() {
  Condition_t Func_ISZERO_ONE_1_0 = 
    {Function, IS_ZERO, ONE, NULL, 1, 0};
{ // EXP @ GLIBC
  trivial_func_info_t info = {
      {DR_REG_XMM0},
      DR_REG_XMM0,
      NULL
  };
  info.condlist = new ConditionList_t*[1];
  info.condlist[0] = new ConditionList_t[1];
  /* Condition_info: is_float, latency, esize, size, check_size */
  info.condlist[0][0].first = {true, 0/*not used*/, 8, 8, 8};
  /* list of Condition_t: attr, val, res, simplify, backward_idx, other_idx */
  info.condlist[0][0].second.push_back(Func_ISZERO_ONE_1_0);
  trivial_func_table[_TRIVIAL_EXP] = info;
}

{ // EXPF @ GLIBC
  trivial_func_info_t info = {
      {DR_REG_XMM0},
      DR_REG_XMM0,
      NULL
  };
  info.condlist = new ConditionList_t*[1];
  info.condlist[0] = new ConditionList_t[1];
  /* Condition_info: is_float, latency, esize, size, check_size */
  info.condlist[0][0].first = {true, 0/*not used*/, 4, 4, 4};
  /* list of Condition_t: attr, val, res, simplify, backward_idx, other_idx */
  info.condlist[0][0].second.push_back(Func_ISZERO_ONE_1_0);
  trivial_func_table[_TRIVIAL_EXPF] = info;
}
}