#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <utility>
#include <regex>
#include <map>
#include <stdlib.h>
#include <time.h>
#include "sat.h"

using namespace std;

// Number of timeframe expansion
const int timeframe = 4;

// Satsolver Basic CNF API
Var addANDCNF(SatSolver& s, Var a, Var b, bool a_bool, bool b_bool) {
    Var out;
    out = s.newVar();
    s.addAigCNF(out, a, a_bool, b, b_bool);
    return out;
}

Var addXORCNF(SatSolver& s, Var a, Var b, bool a_bool, bool b_bool) {
    Var out;
    out = s.newVar();
    s.addXorCNF(out, a, a_bool, b, b_bool);
    return out;
}

Var addORCNF(SatSolver& s, Var a, Var b, bool a_bool, bool b_bool) {
    Var out1, out2;
    out1 = s.newVar();
    s.addAigCNF(out1, a, !a_bool, b, !b_bool);
    out2 = s.newVar();
    s.addAigCNF(out2, out1, true, out1, true);
    return out2;
}

Var addEqualCNF(SatSolver& s, Var a, Var b) {
    Var out1 = addANDCNF(s, a, b, true, true);  // ~a * ~b
    Var out2 = addANDCNF(s, a, b, false, false);    // a * b
    Var out = addORCNF(s, out1, out2, false, false);    // (~a * ~b) + (a * b)
    return out;
}

int getValue(SatSolver& s, Var target) {
    return s.getValue(target);
}

// class of design IO
class Port {
    public:
        Port() {    // default constructor
            port_name = "";
            timeFrameVarList.resize(timeframe);
        }
        ~Port() {;}
        void setName(string name) {
            port_name = name;
        }
        string getName() const {return port_name;}
        // Store Var value of each timeframe of this port
        vector<Var> timeFrameVarList;
    private:
        string port_name;
};

// string manipulation
void trim(string& s) {
    if (s.empty()) {
        return;
    }

    s.erase(0, s.find_first_not_of(" "));
    s.erase(s.find_last_not_of(" ") + 1);
}

void trim_apostrophe(string& s) {
    if (s.empty()) {
        return;
    }

    s.erase(0, s.find_first_not_of("\'"));
    s.erase(s.find_last_not_of("\'") + 1);
}

string trim_inverse(string s) {
    if (s.empty()) {
        return s;
    }

    if (s.find('\'') != std::string::npos) {
        s.erase(s.find_last_not_of("\'") + 1);
    }
    return s;
}

// Check if substring in main string, true -> Yes
bool CheckSubString(string main_string, string subString) {
    if (main_string.find(subString) != std::string::npos) {
        return true;
    }
    else {
        return false;
    }
}

// Create port
Port CreatePort(SatSolver& s, string port_name) {
    Port p = Port();
    p.setName(port_name);
    for (int i=0; i<timeframe; i++) {
        Var v = s.newVar();
        p.timeFrameVarList[i] = v;
    }
    return p;
}

// Assume boolean value to specific port
void AssumePort(SatSolver& s, Port& p, bool val) {
    for (int i=0; i<timeframe; i++) {
        s.assumeProperty(p.timeFrameVarList[i], val);
    }
}

// Assume port value in specific timeframe
void AssumATPGport(SatSolver& solver, Var var_num, char c) {
    if (c == '0') {
        solver.assumeProperty(var_num, 0);
    }
    else if (c == '1') {
        solver.assumeProperty(var_num, 1);
    }
    // cout << var_num << " " << c << endl;

    return;
}

// Read python dictionary and convert it to map
map<string, string> ReadDictionary(string file_name) {
    map<string, string> dict;
    fstream fin;
    fin.open(file_name, ios::in);
    if (!fin) {
        cout << file_name << " can not be opened!" << endl;
        return dict;
    }

    string key, value;
    bool flag = true;  // 0 for value, 1 for key
    char c;

    if (fin >> c && c == '{') {
        while (fin >> c) {
            //cout << c << endl;
            if (c == ':') {
                flag = false;
            }
            else if (c == ',' || c == '}') {
                trim(key);
                trim_apostrophe(key);
                trim(value);
                trim_apostrophe(value);
                dict[key] = value;
                key = "";
                value = "";
                flag = true;
            }
            else {
                if (flag) {
                    key += c;
                }
                else {
                    value += c;
                }
            }
        }
    }
    return dict;
}

Var BuildEquation(SatSolver& solver, fstream& fin, vector<string>& input_list,
    map<string, Port>& port_map, vector<string>& output_list, map<string, string>& DQ_map,
    map<string, string>& DQN_map, map<Var, bool>& base_assume_map) {
    // design module name
    string design_name;
    string read_line;

    // Parse design module name
    if (getline(fin, read_line)) {
        if (CheckSubString(read_line, ".design_name")) {
            cout << "Parse design module name successfully!!!" << endl;
            stringstream ss(read_line);
            string s;
            vector<string> v;
            while (getline(ss, s, ' ')) {
                v.push_back(s);
            }
            design_name = v[1];
            cout << design_name << endl;
        }
    }

    // Parse input port name
    getline(fin, read_line);
    do {
        // cout << read_line << endl;
        stringstream ss(read_line);
        string stage_s;
        while (getline(ss, stage_s, ' ')) {
            if (stage_s != ".inputnames" && stage_s != "") {
                trim(stage_s);
                input_list.push_back(stage_s);
            }
        }
        getline(fin, read_line);
    } while (read_line.find(".inputnames") != std::string::npos);

    // Initialize input port class
    for (int i=0; i<input_list.size(); i++) {
        Port p = CreatePort(solver, input_list[i]);
        // push port class into map
        port_map[input_list[i]] = p;
    }

    cout << "Parse input port name successfully!!!" << endl;

    // Parse output port name
    do {
        //cout << read_line << endl;
        stringstream ss(read_line);
        string stage_s;
        while (getline(ss, stage_s, ' ')) {
            if (stage_s != ".outputnames" && stage_s != "") {
                trim(stage_s);
                output_list.push_back(stage_s);
            }
        }
        getline(fin, read_line);
    } while (read_line.find(".outputnames") != std::string::npos);

    // Initialize output port class
    for (int i=0; i<output_list.size(); i++) {
        Port p = CreatePort(solver, output_list[i]);
        // push port class into map
        port_map[output_list[i]] = p;
    }

    cout << "Parse output port name successfully!!!" << endl;

    // Solver total variable
    Var var_out = solver.newVar();
    solver.assumeProperty(var_out, 1);
    base_assume_map[var_out] = 1;

    // Start parse combinational logic
    do {
        string left_assignment;
        string right_assignment;
        smatch result;
        regex r("([^\n]*?) =");
        // Parse LHS assignment
        if (regex_search(read_line, result, r)) {
            left_assignment = result[0];
            left_assignment.replace(left_assignment.find_first_of(" "), 2, "");
        }

        // if lhs assignment is *Logic0* or *Logic1* -> break
        if (left_assignment.find("Logic") != std::string::npos) {
            break;
        }
        trim(left_assignment);
        // Check if left assignment in port map
        // left assignment not in port map
        if (port_map.find(left_assignment) == port_map.end()) {
            Port p = CreatePort(solver, left_assignment);
            port_map[left_assignment] = p;
        }


        Port lhs_p = port_map[left_assignment];

        // Parse RHS assignemnt
        r = "=.*?;";
        if (regex_search(read_line, result, r)) {
            right_assignment = result[0];
            right_assignment.replace(right_assignment.find_first_of("="), 2, "");
        }
        trim(right_assignment);
        // Initialize operator stack
        stack<char> operator_stack;
        // Initialize operand stack
        stack<string> operand_stack;
        // Intermediate operand variable
        map<string, Port> intermediate_var;

        // operator array
        string match_operator_string = "*+^";

        // left paranthesis flag
        int lhs_paranthesis_count = 0;
        // Intermediate variable count
        int inter_count = 0;
        // cout << read_line << endl;
        // Traverse the whole right assignment
        string stage_string = "";

        // cout << left_assignment << " " << right_assignment << endl;
        for (int i=0; i<right_assignment.length(); i++) {
            //cout << right_assignment[i] << endl;
            // check if it is ';'
            if (right_assignment[i] == ';') {
                // RHS is *Logic0* or *Logic1*
                if (stage_string.find("Logic") != std::string::npos) {
                    bool assume_logic = false;
                    if (stage_string.find("Logic0") != std::string::npos && stage_string.find("\'") != std::string::npos) {
                        assume_logic = true;
                        AssumePort(solver, lhs_p, assume_logic);
                    }
                    else {
                        assume_logic = false;
                        AssumePort(solver, lhs_p, assume_logic);
                    }
                    for (int i=0; i<timeframe; i++) {
                        base_assume_map[lhs_p.timeFrameVarList[i]] = assume_logic;
                    }
                }
                else {  // RHS is single variable
                    trim(stage_string);
                    string pure_port_name = trim_inverse(stage_string);
                    if (port_map.find(pure_port_name) == port_map.end()) {
                        Port p = CreatePort(solver, pure_port_name);
                        port_map[pure_port_name] = p;
                    }
                    Port single_p = port_map[pure_port_name];
                    if (stage_string.find('\'') != std::string::npos) { // inverse variable
                        for (int i=0; i<timeframe; i++) {
                            Var out = addXORCNF(solver, lhs_p.timeFrameVarList[i], single_p.timeFrameVarList[i], false, false);
                            var_out = addANDCNF(solver, var_out, out, false, false);
                        }
                    }
                    else {
                        for (int i=0; i<timeframe; i++) {
                            Var out = addEqualCNF(solver, lhs_p.timeFrameVarList[i], single_p.timeFrameVarList[i]);
                            var_out = addANDCNF(solver, var_out, out, false, false);
                        }
                    }
                }
                break;
            }

            // check if it is '('
            else if (right_assignment[i] == '(') {
                operator_stack.push(right_assignment[i]);
                lhs_paranthesis_count++;
            }

            // if it is an operator
            else if (match_operator_string.find(right_assignment[i]) != std::string::npos) {
                operator_stack.push(right_assignment[i]);
                if (stage_string != " ") {
                    // push stage_string into operand stack
                    // trim stage_string
                    //cout << stage_string << endl;
                    trim(stage_string);
                    //cout << stage_string << endl;
                    string pure_port_name = trim_inverse(stage_string);
                    //cout << pure_port_name << endl;
                    // check if stage_string in port map
                    if (port_map.find(pure_port_name) == port_map.end()) {
                        Port p = CreatePort(solver, pure_port_name);
                        port_map[pure_port_name] = p;
                    }
                    operand_stack.push(stage_string);
                }
                stage_string = "";
            }

            // chech if it is ')'
            else if (right_assignment[i] == ')') {
                // check 2 consecutive right paranthesis
                if (stage_string != "") {
                    // trim stage_string
                    //cout << stage_string << endl;
                    trim(stage_string);
                    //cout << stage_string << endl;
                    string pure_port_name = trim_inverse(stage_string);
                    //cout << pure_port_name << endl;
                    // check if stage_string in port map
                    if (port_map.find(pure_port_name) == port_map.end()) {
                        Port p = CreatePort(solver, pure_port_name);
                        port_map[pure_port_name] = p;
                    }
                    operand_stack.push(stage_string);
                }
                stage_string = "";
                while (!operator_stack.empty())
                {
                    // operator
                    char op = operator_stack.top();
                    operator_stack.pop();
                    if (op == '(') {
                        lhs_paranthesis_count--;
                        break;
                    }

                    // operand1
                    string opnd1;
                    bool inv1 = false;
                    // operand2
                    string opnd2;
                    bool inv2 = false;
                    if (!operand_stack.empty()) {
                        opnd1 = operand_stack.top();
                        operand_stack.pop();
                    }
                    if (!operand_stack.empty()) {
                        opnd2 = operand_stack.top();
                        operand_stack.pop();
                    }

                    // check operand1 is inverted
                    if (opnd1.find('\'') != string::npos) {
                        opnd1.replace(opnd1.find_first_of('\''), 1, "");
                        inv1 = true;
                    }
                    // check operand2 is inverted
                    if (opnd2.find('\'') != string::npos) {
                        opnd2.replace(opnd2.find_first_of('\''), 1, "");
                        inv2 = true;
                    }

                    // port operand1
                    Port p1;
                    // port operand2
                    Port p2;

                    if (port_map.find(opnd1) != port_map.end()) {
                        p1 = port_map[opnd1];
                    }
                    else {
                        p1 = intermediate_var[opnd1];
                    }
                    if (port_map.find(opnd2) != port_map.end()) {
                        p2 = port_map[opnd2];
                    }
                    else {
                        p2 = intermediate_var[opnd2];
                    }

                    // intermediate port
                    string inter_port_name = "inter_" + std::to_string(inter_count);
                    Port p3 = Port();
                    p3.setName(inter_port_name);
                    inter_count++;
                    // cout << inter_port_name << " = " << opnd1 << " " << inv1 << " " << op << " " << opnd2 << " " << inv2 << endl;
                    // Add CNF to solver
                    for (int i=0; i<timeframe; i++) {
                        if (op == '*') {
                            p3.timeFrameVarList[i] = addANDCNF(solver, p1.timeFrameVarList[i], p2.timeFrameVarList[i], inv1, inv2);
                        }
                        else if (op == '+') {
                            p3.timeFrameVarList[i] = addORCNF(solver, p1.timeFrameVarList[i], p2.timeFrameVarList[i], inv1, inv2);
                        }
                        else if (op == '^') {
                            p3.timeFrameVarList[i] = addXORCNF(solver, p1.timeFrameVarList[i], p2.timeFrameVarList[i], inv1, inv2);
                        }
                        else {
                            cout << "Invalid operator!!!" << endl;
                            break;
                        }
                    }
                    // make p3 intermediate var
                    intermediate_var[inter_port_name] = p3;
                    // push intermediate port back to operand stack
                    operand_stack.push(inter_port_name);
                }
                if (lhs_paranthesis_count == 0) {
                    // set last operand in stack equals to lhs variable
                    Port last_p;
                    if (!operand_stack.empty()) {
                        string last_string = operand_stack.top();
                        //cout << last_string << endl;
                        operand_stack.pop();
                        last_p = intermediate_var[last_string];
                    }
                    else {
                        cout << "Invalid computation !!!" << endl;
                    }
                    //cout << lhs_p.getName() << " = " << last_p.getName() << endl;
                    // Handle )' case
                    if (right_assignment[i+1] == '\'') {
                        for (int i=0; i<timeframe; i++) {
                            Var stage_var = addXORCNF(solver, lhs_p.timeFrameVarList[i], last_p.timeFrameVarList[i], false, false);
                            var_out = addANDCNF(solver, var_out, stage_var, false, false);
                        }
                        // cout << read_line << endl;
                        // cout << lhs_p.getName() << " = " << last_p.getName() << endl;
                    }
                    else {
                        for (int i=0; i<timeframe; i++) {
                        Var stage_var = addEqualCNF(solver, lhs_p.timeFrameVarList[i], last_p.timeFrameVarList[i]);
                        var_out = addANDCNF(solver, var_out, stage_var, false, false);
                        }
                    }
                    break;
                }
            }
            else {
                // Normal character
                stage_string += right_assignment[i];
            }
        }
    } while (getline(fin, read_line));

    cout << "Parse internal logic successfully!!!" << endl;

    // Read DQ map
    for (auto i = DQ_map.begin(); i != DQ_map.end(); i++) {
        string D_port = i->first;
        Port Dp;
        if (port_map.find(D_port) != port_map.end()) {
            Dp = port_map[D_port];
        }
        else {
            cout << D_port << " can not be found in port map!!!" << endl;
        }
        string Q_port = i->second;
        Port Qp;
        if (port_map.find(Q_port) != port_map.end()) {
            Qp = port_map[Q_port];
        }
        else {
            cout << Q_port << " can not be found in port map!!!" << endl;
        }
        for (int time=0; time<timeframe - 1; time++) {
            Var stage_var = addEqualCNF(solver, Qp.timeFrameVarList[time], Dp.timeFrameVarList[time+1]);
            var_out = addANDCNF(solver, var_out, stage_var, false, false);
        }
    }

    // Read DQN map
    for (auto i = DQN_map.begin(); i != DQN_map.end(); i++) {
        string D_port = i->first;
        Port Dp;
        if (port_map.find(D_port) != port_map.end()) {
            Dp = port_map[D_port];
        }
        else {
            cout << D_port << " can not be found in port map!!!" << endl;
        }
        string QN_port = i->second;
        Port QNp;
        if (port_map.find(QN_port) != port_map.end()) {
            QNp = port_map[QN_port];
        }
        else {
            cout << QN_port << " can not be found in port map!!!" << endl;
        }
        for (int time=0; time<timeframe-1; time++) {
            Var stage_var = addXORCNF(solver, QNp.timeFrameVarList[time], Dp.timeFrameVarList[time+1], false, false);
            var_out = addANDCNF(solver, var_out, stage_var, false, false);
        }
    }

    cout << "Parse register DQ map successfully!!!" << endl;

    // Set rst_n = 1
    Port p = port_map["rst_n"];
    for (int i=0; i<timeframe; i++) {
        solver.assumeProperty(p.timeFrameVarList[i], 1);
        base_assume_map[p.timeFrameVarList[i]] = 1;
    }

    return var_out;
}

// Make base assumption on boolean formula after assumption release
void ReAssumeBaseAssumption(SatSolver& solver, map<Var, bool>& base_assume_map) {
    for (auto i = base_assume_map.begin(); i != base_assume_map.end(); i++) {
        solver.assumeProperty(i->first, i->second);
    }

    return;
}

// Make ATPG assumption on the target scan registers
void AssumeATPG(SatSolver& solver, map<string, Port>& port_map, map<string, string>& DFF_pipeline_map,
     map<string, string>& DFF_GPR_map, map<string, string>& ATPG_pattern_map, int pattern_idx)
{
    // string of reg or input ATPG assignment
    // Parse I_MEM_rdata string
    string IF1_str = "";
    string IF2_str = "";
    for (int i=31; i>=0 ; i--) {
        string target_str = "I_MEM_rdata[" + to_string(i) + "]";
        string I_MEM_rdata_string = ATPG_pattern_map[target_str];

        if (pattern_idx == 0) {
            IF1_str += I_MEM_rdata_string[pattern_idx];
        }
        else {
            IF1_str += I_MEM_rdata_string[2*pattern_idx - 1];
            IF2_str += I_MEM_rdata_string[2*pattern_idx];
        }
    }
    cout << "In " << pattern_idx << ": " << endl;
    cout << "IF1: " << IF1_str << endl;
    cout << "IF2: " << IF2_str << endl;

    // Parse GPR string
    vector<string> GPR_string;
    for (int GPR_idx=0; GPR_idx<32; GPR_idx++) {
        string cur_GPR_string = "";
        for (int GPR_slice_idx = 31; GPR_slice_idx>=0; GPR_slice_idx--) {
            string target_str = "register_reg_" + to_string(GPR_idx) + "__" + to_string(GPR_slice_idx) + "_";
            string register_reg_string = ATPG_pattern_map[target_str];
            cur_GPR_string += register_reg_string[pattern_idx];
        }
        GPR_string.push_back(cur_GPR_string);
    }
    for (int i=0; i<GPR_string.size(); i++) {
        cout << "GPR" << i << ": " << GPR_string[i] << endl;
    }

    // Parse IF_ID pipeline reg
    char IF_ID_compress_o_reg_str;
    string IF_ID_instr_o_reg_str = "";
    IF_ID_compress_o_reg_str = ATPG_pattern_map["IF_ID_compress_o_reg"][pattern_idx];
    AssumATPGport(solver, port_map[DFF_pipeline_map["IF_ID_compress_o_reg"]].timeFrameVarList[0], IF_ID_compress_o_reg_str);
    for (int i=31; i>=0; i--) {
        string target_str = "IF_ID_instr_o_reg_" + to_string(i) + "_";
        char cur_pattern_str = ATPG_pattern_map[target_str][pattern_idx];
        IF_ID_instr_o_reg_str += cur_pattern_str;
        AssumATPGport(solver, port_map[DFF_pipeline_map[target_str]].timeFrameVarList[0], cur_pattern_str);
    }
    cout << "IF_ID_compress_o_reg: " << IF_ID_compress_o_reg_str << endl;
    cout << "IF_ID_instr_o_reg: " << IF_ID_instr_o_reg_str << endl;

    // Parse ID_EX pipeline reg
    string ID_EX_EX_ALUOp_o_reg_str = "";
    for (int i=6; i>=0; i--) {
        string target_str = "ID_EX_EX_ALUOp_o_reg_" + to_string(i) + "_";
        char cur_pattern_str = ATPG_pattern_map[target_str][pattern_idx];
        ID_EX_EX_ALUOp_o_reg_str += cur_pattern_str;
        AssumATPGport(solver, port_map[DFF_pipeline_map[target_str]].timeFrameVarList[0], cur_pattern_str);
    }
    cout << "ID_EX_EX_ALUOp_o_reg: " << ID_EX_EX_ALUOp_o_reg_str << endl;
    char ID_EX_EX_ALUSrc_o_reg_str;
    ID_EX_EX_ALUSrc_o_reg_str = ATPG_pattern_map["ID_EX_EX_ALUSrc_o_reg"][pattern_idx];
    AssumATPGport(solver, port_map[DFF_pipeline_map["ID_EX_EX_ALUSrc_o_reg"]].timeFrameVarList[0], ID_EX_EX_ALUSrc_o_reg_str);
    cout << "ID_EX_EX_ALUSrc_o_reg: " << ID_EX_EX_ALUSrc_o_reg_str << endl;
    char ID_EX_EX_Jalr_o_reg_str;
    ID_EX_EX_Jalr_o_reg_str = ATPG_pattern_map["ID_EX_EX_Jalr_o_reg"][pattern_idx];
    AssumATPGport(solver, port_map[DFF_pipeline_map["ID_EX_EX_Jalr_o_reg"]].timeFrameVarList[0], ID_EX_EX_Jalr_o_reg_str);
    cout << "ID_EX_EX_Jalr_o_reg: " << ID_EX_EX_Jalr_o_reg_str << endl;
    char ID_EX_M_MemRead_o_reg_str;
    ID_EX_M_MemRead_o_reg_str = ATPG_pattern_map["ID_EX_M_MemRead_o_reg"][pattern_idx];
    AssumATPGport(solver, port_map[DFF_pipeline_map["ID_EX_M_MemRead_o_reg"]].timeFrameVarList[0], ID_EX_M_MemRead_o_reg_str);
    cout << "ID_EX_M_MemRead_o_reg: " << ID_EX_M_MemRead_o_reg_str << endl;
    char ID_EX_M_MemWrite_o_reg_str;
    ID_EX_M_MemWrite_o_reg_str = ATPG_pattern_map["ID_EX_M_MemWrite_o_reg"][pattern_idx];
    AssumATPGport(solver, port_map[DFF_pipeline_map["ID_EX_M_MemWrite_o_reg"]].timeFrameVarList[0], ID_EX_M_MemWrite_o_reg_str);
    cout << "ID_EX_M_MemWrite_o_reg: " << ID_EX_M_MemWrite_o_reg_str << endl;
    string ID_EX_RDaddr_o_reg_str = "";
    for (int i=4; i>=0; i--) {
        string target_str = "ID_EX_RDaddr_o_reg_" + to_string(i) + "_";
        char cur_pattern_str = ATPG_pattern_map[target_str][pattern_idx];
        ID_EX_RDaddr_o_reg_str += cur_pattern_str;
        AssumATPGport(solver, port_map[DFF_pipeline_map[target_str]].timeFrameVarList[0], cur_pattern_str);
    }
    cout << "ID_EX_RDaddr_o_reg: " << ID_EX_RDaddr_o_reg_str << endl;
    string ID_EX_RS1addr_o_reg_str = "";
    for (int i=4; i>=0; i--) {
        string target_str = "ID_EX_RS1addr_o_reg_" + to_string(i) + "_";
        char cur_pattern_str = ATPG_pattern_map[target_str][pattern_idx];
        ID_EX_RS1addr_o_reg_str += cur_pattern_str;
        AssumATPGport(solver, port_map[DFF_pipeline_map[target_str]].timeFrameVarList[0], cur_pattern_str);
    }
    cout << "ID_EX_RS1addr_o_reg: " << ID_EX_RS1addr_o_reg_str << endl;
    string ID_EX_RS1data_o_reg_str = "";
    for (int i=31; i>=0; i--) {
        string target_str = "ID_EX_RS1data_o_reg_" + to_string(i) + "_";
        char cur_pattern_str = ATPG_pattern_map[target_str][pattern_idx];
        ID_EX_RS1data_o_reg_str += cur_pattern_str;
        AssumATPGport(solver, port_map[DFF_pipeline_map[target_str]].timeFrameVarList[0], cur_pattern_str);
    }
    cout << "ID_EX_RS1data_o_reg: " << ID_EX_RS1data_o_reg_str << endl;
    string ID_EX_RS2addr_o_reg_str = "";
    for (int i=4; i>=0; i--) {
        string target_str = "ID_EX_RS2addr_o_reg_" + to_string(i) + "_";
        char cur_pattern_str = ATPG_pattern_map[target_str][pattern_idx];
        ID_EX_RS2addr_o_reg_str += cur_pattern_str;
        AssumATPGport(solver, port_map[DFF_pipeline_map[target_str]].timeFrameVarList[0], cur_pattern_str);
    }
    cout << "ID_EX_RS2addr_o_reg: " << ID_EX_RS2addr_o_reg_str << endl;
    string ID_EX_RS2data_o_reg_str = "";
    for (int i=31; i>=0; i--) {
        string target_str = "ID_EX_RS2data_o_reg_" + to_string(i) + "_";
        char cur_pattern_str = ATPG_pattern_map[target_str][pattern_idx];
        ID_EX_RS2data_o_reg_str += cur_pattern_str;
        AssumATPGport(solver, port_map[DFF_pipeline_map[target_str]].timeFrameVarList[0], cur_pattern_str);
    }
    cout << "ID_EX_RS2data_o_reg: " << ID_EX_RS2data_o_reg_str << endl;
    string ID_EX_WB_MemtoReg_o_reg_str = "";
    for (int i=1; i>=0; i--) {
        string target_str = "ID_EX_WB_MemtoReg_o_reg_" + to_string(i) + "_";
        char cur_pattern_str = ATPG_pattern_map[target_str][pattern_idx];
        ID_EX_WB_MemtoReg_o_reg_str += cur_pattern_str;
        AssumATPGport(solver, port_map[DFF_pipeline_map[target_str]].timeFrameVarList[0], cur_pattern_str);
    }
    cout << "ID_EX_WB_MemtoReg_o_reg: " << ID_EX_WB_MemtoReg_o_reg_str << endl;
    char ID_EX_WB_RegWrite_o_reg_str;
    ID_EX_WB_RegWrite_o_reg_str = ATPG_pattern_map["ID_EX_WB_RegWrite_o_reg"][pattern_idx];
    AssumATPGport(solver, port_map[DFF_pipeline_map["ID_EX_WB_RegWrite_o_reg"]].timeFrameVarList[0], ID_EX_WB_RegWrite_o_reg_str);
    cout << "ID_EX_WB_RegWrite_o_reg: " << ID_EX_WB_RegWrite_o_reg_str << endl;
    char ID_EX_compress_o_reg_str;
    ID_EX_compress_o_reg_str = ATPG_pattern_map["ID_EX_compress_o_reg"][pattern_idx];
    AssumATPGport(solver, port_map[DFF_pipeline_map["ID_EX_compress_o_reg"]].timeFrameVarList[0], ID_EX_compress_o_reg_str);
    cout << "ID_EX_compress_o_reg: " << ID_EX_compress_o_reg_str << endl;
    string ID_EX_funct3_o_reg_str = "";
    for (int i=2; i>=0; i--) {
        string target_str = "ID_EX_funct3_o_reg_" + to_string(i) + "_";
        char cur_pattern_str = ATPG_pattern_map[target_str][pattern_idx];
        ID_EX_funct3_o_reg_str += cur_pattern_str;
        AssumATPGport(solver, port_map[DFF_pipeline_map[target_str]].timeFrameVarList[0], cur_pattern_str);
    }
    cout << "ID_EX_funct3_o_reg: " << ID_EX_funct3_o_reg_str << endl;
    char ID_EX_funct7_o_reg_str;
    ID_EX_funct7_o_reg_str = ATPG_pattern_map["ID_EX_funct7_o_reg"][pattern_idx];
    AssumATPGport(solver, port_map[DFF_pipeline_map["ID_EX_funct7_o_reg"]].timeFrameVarList[0], ID_EX_funct7_o_reg_str);
    cout << "ID_EX_funct7_o_reg: " << ID_EX_funct7_o_reg_str << endl;
    string ID_EX_imm_o_reg_str = "";
    for (int i=21; i>=0; i--) {
        string target_str = "";
        if (i == 21) {
            target_str = "ID_EX_imm_o_reg_31_"; 
        }
        else {
            target_str = "ID_EX_imm_o_reg_" + to_string(i) + "_";
        }
        char cur_pattern_str = ATPG_pattern_map[target_str][pattern_idx];
        ID_EX_imm_o_reg_str += cur_pattern_str;
        AssumATPGport(solver, port_map[DFF_pipeline_map[target_str]].timeFrameVarList[0], cur_pattern_str);
    }
    cout << "ID_EX_imm_o_reg: " << ID_EX_imm_o_reg_str << endl;

    // Parse EX_MEM pipeline reg
    string ALU_result_o_reg_str = "";
    for (int i=31; i>=0; i--) {
        string target_str = "EX_MEM_ALU_result_o_reg_" + to_string(i) + "_";
        char cur_pattern_str = ATPG_pattern_map[target_str][pattern_idx];
        ALU_result_o_reg_str += cur_pattern_str;
        AssumATPGport(solver, port_map[DFF_pipeline_map[target_str]].timeFrameVarList[0], cur_pattern_str);
    }
    cout << "EX_MEM_ALU_result_o_reg: " << ALU_result_o_reg_str << endl;
    char EX_MEM_MEM_reg_hazard_o_reg_str;
    EX_MEM_MEM_reg_hazard_o_reg_str = ATPG_pattern_map["EX_MEM_MEM_reg_hazard_o_reg"][pattern_idx];
    AssumATPGport(solver, port_map[DFF_pipeline_map["EX_MEM_MEM_reg_hazard_o_reg"]].timeFrameVarList[0], EX_MEM_MEM_reg_hazard_o_reg_str);
    cout << "EX_MEM_MEM_reg_hazard_o_reg: " << EX_MEM_MEM_reg_hazard_o_reg_str << endl;
    char EX_MEM_M_MemRead_o_reg_str;
    EX_MEM_M_MemRead_o_reg_str = ATPG_pattern_map["EX_MEM_M_MemRead_o_reg"][pattern_idx];
    AssumATPGport(solver, port_map[DFF_pipeline_map["EX_MEM_M_MemRead_o_reg"]].timeFrameVarList[0], EX_MEM_M_MemRead_o_reg_str);
    cout << "EX_MEM_M_MemRead_o_reg: " << EX_MEM_M_MemRead_o_reg_str << endl;
    char EX_MEM_M_MemWrite_o_reg_str;
    EX_MEM_M_MemWrite_o_reg_str = ATPG_pattern_map["EX_MEM_M_MemWrite_o_reg"][pattern_idx];
    AssumATPGport(solver, port_map[DFF_pipeline_map["EX_MEM_M_MemWrite_o_reg"]].timeFrameVarList[0], EX_MEM_M_MemWrite_o_reg_str);
    cout << "EX_MEM_M_MemWrite_o_reg: " << EX_MEM_M_MemWrite_o_reg_str << endl;
    string EX_MEM_RDaddr_o_reg_str = "";
    for (int i=4; i>=0; i--) {
        string target_str = "EX_MEM_RDaddr_o_reg_" + to_string(i) + "_";
        char cur_pattern_str = ATPG_pattern_map[target_str][pattern_idx];
        EX_MEM_RDaddr_o_reg_str += cur_pattern_str;
        AssumATPGport(solver, port_map[DFF_pipeline_map[target_str]].timeFrameVarList[0], cur_pattern_str);
    }
    cout << "EX_MEM_RDaddr_o_reg: " << EX_MEM_RDaddr_o_reg_str << endl;
    string EX_MEM_RS2data_o_reg_str = "";
    for (int i=31; i>=0; i--) {
        string target_str = "EX_MEM_RS2data_o_reg_" + to_string(i) + "_";
        char cur_pattern_str = ATPG_pattern_map[target_str][pattern_idx];
        EX_MEM_RS2data_o_reg_str += cur_pattern_str;
        AssumATPGport(solver, port_map[DFF_pipeline_map[target_str]].timeFrameVarList[0], cur_pattern_str); 
    }
    cout << "EX_MEM_RS2data_o_reg: " << EX_MEM_RS2data_o_reg_str << endl;
    string EX_MEM_WB_MemtoReg_o_reg_str = "";
    for (int i=1; i>=0; i--) {
        string target_str = "EX_MEM_WB_MemtoReg_o_reg_" + to_string(i) + "_";
        char cur_pattern_str = ATPG_pattern_map[target_str][pattern_idx];
        EX_MEM_WB_MemtoReg_o_reg_str += cur_pattern_str;
        AssumATPGport(solver, port_map[DFF_pipeline_map[target_str]].timeFrameVarList[0], cur_pattern_str);
    }
    cout << "EX_MEM_WB_MemtoReg_o_reg: " << EX_MEM_WB_MemtoReg_o_reg_str << endl;
    char EX_MEM_WB_RegWrite_o_reg_str;
    EX_MEM_WB_RegWrite_o_reg_str = ATPG_pattern_map["EX_MEM_WB_RegWrite_o_reg"][pattern_idx];
    AssumATPGport(solver, port_map[DFF_pipeline_map["EX_MEM_WB_RegWrite_o_reg"]].timeFrameVarList[0], EX_MEM_WB_RegWrite_o_reg_str);
    cout << "EX_MEM_WB_RegWrite_o_reg: " << EX_MEM_WB_RegWrite_o_reg_str << endl;

    // Parse MEM_WB pipeline reg
    string MEM_WB_ALU_result_o_reg_str = "";
    for (int i=31; i>=0; i--) {
        string target_str = "MEM_WB_ALU_result_o_reg_" + to_string(i) + "_";
        char cur_pattern_str = ATPG_pattern_map[target_str][pattern_idx];
        MEM_WB_ALU_result_o_reg_str += cur_pattern_str;
        AssumATPGport(solver, port_map[DFF_pipeline_map[target_str]].timeFrameVarList[0], cur_pattern_str);
    }
    cout << "MEM_WB_ALU_result_o_reg: " << MEM_WB_ALU_result_o_reg_str << endl;
    string MEM_WB_RDaddr_o_reg_str = "";
    for (int i=4; i>=0; i--) {
        string target_str = "MEM_WB_RDaddr_o_reg_" + to_string(i) + "_";
        char cur_pattern_str = ATPG_pattern_map[target_str][pattern_idx];
        MEM_WB_RDaddr_o_reg_str += cur_pattern_str;
        AssumATPGport(solver, port_map[DFF_pipeline_map[target_str]].timeFrameVarList[0], cur_pattern_str);
    }
    cout << "MEM_WB_RDaddr_o_reg: " << MEM_WB_RDaddr_o_reg_str << endl;
    string MEM_WB_WB_MemtoReg_o_reg_str = "";
    for (int i=1; i>=0; i--) {
        string target_str = "MEM_WB_WB_MemtoReg_o_reg_" + to_string(i) + "_";
        char cur_pattern_str = ATPG_pattern_map[target_str][pattern_idx];
        MEM_WB_WB_MemtoReg_o_reg_str += cur_pattern_str;
        AssumATPGport(solver, port_map[DFF_pipeline_map[target_str]].timeFrameVarList[0], cur_pattern_str);
    }
    cout << "MEM_WB_WB_MemtoReg_o_reg: " << MEM_WB_WB_MemtoReg_o_reg_str << endl;
    char MEM_WB_WB_RegWrite_o_reg_str;
    MEM_WB_WB_RegWrite_o_reg_str = ATPG_pattern_map["MEM_WB_WB_RegWrite_o_reg"][pattern_idx];
    AssumATPGport(solver, port_map[DFF_pipeline_map["MEM_WB_WB_RegWrite_o_reg"]].timeFrameVarList[0], MEM_WB_WB_RegWrite_o_reg_str);
    cout << "MEM_WB_WB_RegWrite_o_reg: " << MEM_WB_WB_RegWrite_o_reg_str << endl;
    string MEM_WB_data_o_reg_str = "";
    for (int i=31; i>=0; i--) {
        string target_str = "MEM_WB_data_o_reg_" + to_string(i) + "_";
        char cur_pattern_str = ATPG_pattern_map[target_str][pattern_idx];
        MEM_WB_data_o_reg_str += cur_pattern_str;
        AssumATPGport(solver, port_map[DFF_pipeline_map[target_str]].timeFrameVarList[0], cur_pattern_str);
    }
    cout << "MEM_WB_data_o_reg: " << MEM_WB_data_o_reg_str << endl;
    

    return;
}

int main(int argc, char* argv[]) {
    fstream fin;
    fstream fout;
    // Input arguments
    string exec_name = argv[0];
    string equation_file_name = argv[1];
    string output_file_name = argv[2];
    string DQ_map_file_name = argv[3];
    string DQN_map_file_name = argv[4];
    string DFF_pipeline_map_file_name = argv[5];
    string DFF_GPR_map_file_name = argv[6];
    string ATPG_pattern_name = argv[7];
    // Open input/output files
    fin.open(equation_file_name, ios::in);
    if (!fin) {
        cout << endl;
        cout << equation_file_name << " can not be opened! " << endl;
    }
    fout.open(output_file_name, ios::out);

    // Random number generator
    int seed = 35;
    srand(seed);

    // Initialize SAT solver
    SatSolver solver;
    solver.initialize();

    // input port name list
    vector<string> input_list;
    // Port map
    map<string, Port> port_map;
    // output port name list
    vector<string> output_list;

    // Read DQ map
    map<string, string> DQ_map = ReadDictionary(DQ_map_file_name);
    // Read DQN map
    map<string, string> DQN_map = ReadDictionary(DQN_map_file_name);

    // Read DFF map
    map<string, string> DFF_pipeline_map = ReadDictionary(DFF_pipeline_map_file_name);
    map<string, string> DFF_GPR_map = ReadDictionary(DFF_GPR_map_file_name);

    // Read ATPG pattern
    map<string, string> ATPG_pattern_map = ReadDictionary(ATPG_pattern_name);

    // Record Base assumption var list
    map<Var, bool> base_assume_map;

    // Build SAT equation
    Var var_out = BuildEquation(solver, fin, input_list, port_map, output_list, DQ_map, DQN_map, base_assume_map);

    cout << "Building circuit equation successfully!!!" << endl;

    int ATPG_pattern_count = ATPG_pattern_map["register_reg_25__28_"].size();
    int SAT_pattern_count = 0;
    // int ATPG_pattern_count = 5;
    cout << ATPG_pattern_count << " of ATPG pattern to be converted!!!" << endl;

    for (int i=0; i<ATPG_pattern_count; i++) {
        AssumeATPG(solver, port_map, DFF_pipeline_map, DFF_GPR_map, ATPG_pattern_map, i);

        // Set Satisfiable & solve the input variable
        solver.assumeProperty(var_out, true);
        bool result = solver.assumpSolve();
        solver.printStats();
        cout << (result ? "SAT" : "UNSAT") << endl;
        SAT_pattern_count = (result) ? SAT_pattern_count + 1 : SAT_pattern_count;

        // clear all the assumption & reassume base assumption
        solver.assumeRelease();
        ReAssumeBaseAssumption(solver, base_assume_map);
    }

    cout << "SATISFIABLE scan pattern / Total scan pattern: " << SAT_pattern_count << "/" << ATPG_pattern_count << endl;

    // Set IF-ID reg 11100101000000010000000100010011
    // cout << port_map[DFF_pipeline_map["IF_ID_instr_o_reg_0_"]].timeFrameVarList[0] << endl;
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_0_"]].timeFrameVarList[0], 1);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_1_"]].timeFrameVarList[0], 1);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_2_"]].timeFrameVarList[0], 0);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_3_"]].timeFrameVarList[0], 0);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_4_"]].timeFrameVarList[0], 1);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_5_"]].timeFrameVarList[0], 0);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_6_"]].timeFrameVarList[0], 0);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_7_"]].timeFrameVarList[0], 0);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_8_"]].timeFrameVarList[0], 1);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_9_"]].timeFrameVarList[0], 0);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_10_"]].timeFrameVarList[0], 0);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_11_"]].timeFrameVarList[0], 0);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_12_"]].timeFrameVarList[0], 0);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_13_"]].timeFrameVarList[0], 0);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_14_"]].timeFrameVarList[0], 0);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_15_"]].timeFrameVarList[0], 0);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_16_"]].timeFrameVarList[0], 1);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_17_"]].timeFrameVarList[0], 0);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_18_"]].timeFrameVarList[0], 0);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_19_"]].timeFrameVarList[0], 0);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_20_"]].timeFrameVarList[0], 0);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_21_"]].timeFrameVarList[0], 0);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_22_"]].timeFrameVarList[0], 0);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_23_"]].timeFrameVarList[0], 0);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_24_"]].timeFrameVarList[0], 1);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_25_"]].timeFrameVarList[0], 0);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_26_"]].timeFrameVarList[0], 1);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_27_"]].timeFrameVarList[0], 0);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_28_"]].timeFrameVarList[0], 0);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_29_"]].timeFrameVarList[0], 1);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_30_"]].timeFrameVarList[0], 1);
    // solver.assumeProperty(port_map[DFF_pipeline_map["IF_ID_instr_o_reg_31_"]].timeFrameVarList[0], 1);


    // for (int i=0; i<output_list.size(); i++) {
    //     int x = rand() % 2;
    //     bool f = (x == 1) ? 1 : 0;
    //     solver.assumeProperty(port_map[output_list[i]].timeFrameVarList[0], f);
    // } 


    // for (auto i=DFF_pipeline_map.begin(); i != DFF_pipeline_map.end(); i++) {
    //     string DFF_name = i->first;
    //     string reg_port_name = i->second;
    //     bool pc_flag = false;
    //     pc_flag = CheckSubString(DFF_name, "pc") ? true : pc_flag;
    //     pc_flag = CheckSubString(DFF_name, "PC") ? true : pc_flag;
    //     pc_flag = CheckSubString(DFF_name, "MemRead") ? true : pc_flag;
    //     pc_flag = CheckSubString(DFF_name, "MemWrite") ? true : pc_flag;
    //     pc_flag = CheckSubString(DFF_name, "RegWrite") ? true : pc_flag;
    //     pc_flag = CheckSubString(DFF_name, "to") ? true : pc_flag;
    //     pc_flag = CheckSubString(DFF_name, "EX_MEM_MEM_reg_hazard_o_reg") ? true : pc_flag;
    //     pc_flag = CheckSubString(DFF_name, "ID_EX_EX_ALUSrc_o_reg") ? true : pc_flag;
    //     pc_flag = CheckSubString(DFF_name, "ID_EX_EX_Jalr_o_reg") ? true : pc_flag;
    //     pc_flag = CheckSubString(DFF_name, "ID_EX_compress_o_reg") ? true : pc_flag;
    //     pc_flag = CheckSubString(DFF_name, "ID_EX_is_lui_o_reg") ? true : pc_flag;
    //     pc_flag = CheckSubString(DFF_name, "IF_ID_compress_o_reg") ? true : pc_flag;
    //     pc_flag = CheckSubString(DFF_name, "MEM_WB") ? true : pc_flag;
    //     pc_flag = CheckSubString(DFF_name, "EX_MEM") ? true : pc_flag;
    //     //pc_flag = CheckSubString(DFF_name, "ID_EX") ? true : pc_flag;
    //     if (!pc_flag) {
    //         // cout << DFF_name << " " << reg_port_name << endl;
    //         int x = rand() % 2;
    //         bool f = (x == 1) ? true : false;
    //         solver.assumeProperty(port_map[reg_port_name].timeFrameVarList[0], f);
    //     }
    // }

    // for (auto i=DFF_GPR_map.begin(); i != DFF_GPR_map.end(); i++) {
    //     cout << i->first << " " << i->second << endl;
    // }

    solver.assumeProperty(var_out, true);
    bool result = solver.assumpSolve();
    solver.printStats();
    cout << (result ? "SAT" : "UNSAT") << endl;

    // Print Satisfiable input value
    // string ctrl_str = "0000";
    // for (int i=0; i<4; i++) {
    //     int val = getValue(solver, port_map["ctrl[" + std::to_string(i) + "]"].timeFrameVarList[0]);
    //     char v = (val == 0) ? '0' : '1';
    //     ctrl_str[3-i] = v;
    // }
    // cout << "ctrl: " << ctrl_str << endl;
    // string x_str = "00000000";
    // for (int i=0; i<8; i++) {
    //     int val = getValue(solver, port_map["x[" + std::to_string(i) + "]"].timeFrameVarList[0]);
    //     char v = (val == 0) ? '0' : '1';
    //     x_str[7-i] = v;
    // }
    // cout << "x: " << x_str << endl;
    // string y_str = "00000000";
    // for (int i=0; i<8; i++) {
    //     int val = getValue(solver, port_map["y[" + std::to_string(i) + "]"].timeFrameVarList[0]);
    //     char v = (val == 0) ? '0' : '1';
    //     y_str[7-i] = v;
    // }
    // cout << "y: " << y_str << endl;

    return 0;
}
