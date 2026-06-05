#ifndef __qiskitcpp_circuit_qasm3_exporter_hpp__
#define __qiskitcpp_circuit_qasm3_exporter_hpp__

#include "circuit/quantumcircuit_def.hpp"
#include <iostream>
#include <set>

std::string to_qasm3(circuit::QuantumCircuit &circ)
{
    auto rust_circ = circ.get_rust_circuit(true);

    std::stringstream qasm3;
    qasm3 << std::setprecision(18);
    qasm3 << "OPENQASM 3.0;" << std::endl;
    qasm3 << "include \"stdgates.inc\";" << std::endl;
    auto name_map = get_standard_gate_name_mapping();

    // Input parameter declarations
    uint_t num_params = circ.num_parameters();
    if (num_params > 0)
    {
        std::set<std::string> symbol_names;
        uint_t param_insts = qk_circuit_num_instructions(rust_circ.get());

        for (uint_t i = 0; i < param_insts; i++)
        {
            QkCircuitInstruction inst;
            qk_circuit_get_instruction(rust_circ.get(), i, &inst);

            for (uint_t j = 0; j < inst.num_params; j++)
            {
                char *param_str = qk_param_str(inst.params[j]);
                std::string s(param_str);
                qk_str_free(param_str);

                // Extract all identifier tokens from the param string.
                static const std::set<std::string> reserved = {
                    "sin", "cos", "tan", "asin", "acos", "atan", "exp", "log",
                    "abs", "sign", "conjugate", "pi", "sqrt", "ceil", "floor"};

                size_t pos = 0;
                while (pos < s.size())
                {
                    char c = s[pos];
                    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')
                    {
                        size_t start = pos;
                        pos++;
                        while (pos < s.size() &&
                               ((s[pos] >= 'a' && s[pos] <= 'z') ||
                                (s[pos] >= 'A' && s[pos] <= 'Z') ||
                                (s[pos] >= '0' && s[pos] <= '9') || s[pos] == '_'))
                        {
                            pos++;
                        }
                        std::string token = s.substr(start, pos - start);
                        if (reserved.find(token) == reserved.end())
                        {
                            symbol_names.insert(token);
                        }
                        else
                            pos++;
                    }
                }
            }
            qk_circuit_instruction_clear(&inst);
        }

        for (const auto &sym : symbol_names)
        {
            qasm3 << "input float[64] " << sym << ";" << std::endl;
        }
    }

    // add header for non-standard gates
    bool cs = false;
    bool sxdg = false;
    QkOpCounts opcounts = qk_circuit_count_ops(rust_circ.get());
    for (int i = 0; i < opcounts.len; i++)
    {
        if (opcounts.data[i].count != 0)
        {
            auto op = name_map[opcounts.data[i].name].gate_map();
            switch (op)
            {
            case QkGate_R:
                qasm3 << "gate r(p0, p1) _gate_q_0 {" << std::endl;
                qasm3 << "  U(p0, -pi/2 + p1, pi/2 - p1) _gate_q_0;" << std::endl;
                qasm3 << "}" << std::endl;
                break;
            case QkGate_SXdg:
            case QkGate_RYY:
            case QkGate_XXPlusYY:
            case QkGate_XXMinusYY:
                if (!sxdg)
                {
                    qasm3 << "gate sxdg _gate_q_0 {" << std::endl;
                    qasm3 << "  s _gate_q_0;" << std::endl;
                    qasm3 << "  h _gate_q_0;" << std::endl;
                    qasm3 << "  s _gate_q_0;" << std::endl;
                    qasm3 << "}" << std::endl;
                    sxdg = true;
                }
                if (op == QkGate_RYY)
                {
                    qasm3 << "gate ryy(p0) _gate_q_0, _gate_q_1 {" << std::endl;
                    qasm3 << "  sxdg _gate_q_0;" << std::endl;
                    qasm3 << "  sxdg _gate_q_1;" << std::endl;
                    qasm3 << "  cx _gate_q_0, _gate_q_1;" << std::endl;
                    qasm3 << "  rz(p0) _gate_q_1;" << std::endl;
                    qasm3 << "  cx _gate_q_0, _gate_q_1;" << std::endl;
                    qasm3 << "  sx _gate_q_0;" << std::endl;
                    qasm3 << "  sx _gate_q_1;" << std::endl;
                    qasm3 << "}" << std::endl;
                }
                if (op == QkGate_XXPlusYY)
                {
                    qasm3 << "gate xx_plus_yy(p0, p1) _gate_q_0, _gate_q_1 {"
                          << std::endl;
                    qasm3 << "  rz(p1) _gate_q_0;" << std::endl;
                    qasm3 << "  sdg _gate_q_1;" << std::endl;
                    qasm3 << "  sx _gate_q_1;" << std::endl;
                    qasm3 << "  s _gate_q_1;" << std::endl;
                    qasm3 << "  s _gate_q_0;" << std::endl;
                    qasm3 << "  cx _gate_q_1, _gate_q_0;" << std::endl;
                    qasm3 << "  ry((-0.5)*p0) _gate_q_1;" << std::endl;
                    qasm3 << "  ry((-0.5)*p0) _gate_q_0;" << std::endl;
                    qasm3 << "  cx _gate_q_1, _gate_q_0;" << std::endl;
                    qasm3 << "  sdg _gate_q_0;" << std::endl;
                    qasm3 << "  sdg _gate_q_1;" << std::endl;
                    qasm3 << "  sxdg _gate_q_1;" << std::endl;
                    qasm3 << "  s _gate_q_1;" << std::endl;
                    qasm3 << "  rz(-p1) _gate_q_0;" << std::endl;
                    qasm3 << "}" << std::endl;
                }
                if (op == QkGate_XXMinusYY)
                {
                    qasm3 << "gate xx_minus_yy(p0, p1) _gate_q_0, _gate_q_1 {"
                          << std::endl;
                    qasm3 << "  rz(-p1) _gate_q_1;" << std::endl;
                    qasm3 << "  sdg _gate_q_0;" << std::endl;
                    qasm3 << "  sx _gate_q_0;" << std::endl;
                    qasm3 << "  s _gate_q_0;" << std::endl;
                    qasm3 << "  s _gate_q_1;" << std::endl;
                    qasm3 << "  cx _gate_q_0, _gate_q_1;" << std::endl;
                    qasm3 << "  ry(0.5*p0) _gate_q_0;" << std::endl;
                    qasm3 << "  ry((-0.5)*p0) _gate_q_1;" << std::endl;
                    qasm3 << "  cx _gate_q_0, _gate_q_1;" << std::endl;
                    qasm3 << "  sdg _gate_q_1;" << std::endl;
                    qasm3 << "  sdg _gate_q_0;" << std::endl;
                    qasm3 << "  sxdg _gate_q_0;" << std::endl;
                    qasm3 << "  s _gate_q_0;" << std::endl;
                    qasm3 << "  rz(p1) _gate_q_1;" << std::endl;
                    qasm3 << "}" << std::endl;
                }
                break;
            case QkGate_DCX:
                qasm3 << "gate dcx _gate_q_0, _gate_q_1 {" << std::endl;
                qasm3 << "  cx _gate_q_0, _gate_q_1;" << std::endl;
                qasm3 << "  cx _gate_q_1, _gate_q_0;" << std::endl;
                qasm3 << "}" << std::endl;
                break;
            case QkGate_ECR:
                qasm3 << "gate ecr _gate_q_0, _gate_q_1 {" << std::endl;
                qasm3 << "  s _gate_q_0;" << std::endl;
                qasm3 << "  sx _gate_q_1;" << std::endl;
                qasm3 << "  cx _gate_q_0, _gate_q_1;" << std::endl;
                qasm3 << "  x _gate_q_0;" << std::endl;
                qasm3 << "}" << std::endl;
                break;
            case QkGate_ISwap:
                qasm3 << "gate iswap _gate_q_0, _gate_q_1 {" << std::endl;
                qasm3 << "  s _gate_q_0;" << std::endl;
                qasm3 << "  s _gate_q_1;" << std::endl;
                qasm3 << "  h _gate_q_0;" << std::endl;
                qasm3 << "  cx _gate_q_0, _gate_q_1;" << std::endl;
                qasm3 << "  cx _gate_q_1, _gate_q_0;" << std::endl;
                qasm3 << "  h _gate_q_1;" << std::endl;
                qasm3 << "}" << std::endl;
                break;
            case QkGate_CSX:
            case QkGate_CS:
                if (!cs)
                {
                    qasm3 << "gate cs _gate_q_0, _gate_q_1 {" << std::endl;
                    qasm3 << "  t _gate_q_0;" << std::endl;
                    qasm3 << "  cx _gate_q_0, _gate_q_1;" << std::endl;
                    qasm3 << "  tdg _gate_q_1;" << std::endl;
                    qasm3 << "  cx _gate_q_0, _gate_q_1;" << std::endl;
                    qasm3 << "  t _gate_q_1;" << std::endl;
                    qasm3 << "}" << std::endl;
                    cs = true;
                }
                if (op == QkGate_CSX)
                {
                    qasm3 << "gate csx _gate_q_0, _gate_q_1 {" << std::endl;
                    qasm3 << "  h _gate_q_1;" << std::endl;
                    qasm3 << "  cs _gate_q_0, _gate_q_1;" << std::endl;
                    qasm3 << "  h _gate_q_1;" << std::endl;
                    qasm3 << "}" << std::endl;
                }
                break;
            case QkGate_CSdg:
                qasm3 << "gate csdg _gate_q_0, _gate_q_1 {" << std::endl;
                qasm3 << "  tdg _gate_q_0;" << std::endl;
                qasm3 << "  cx _gate_q_0, _gate_q_1;" << std::endl;
                qasm3 << "  t _gate_q_1;" << std::endl;
                qasm3 << "  cx _gate_q_0, _gate_q_1;" << std::endl;
                qasm3 << "  tdg _gate_q_1;" << std::endl;
                qasm3 << "}" << std::endl;
                break;
            case QkGate_CCZ:
                qasm3 << "gate ccz _gate_q_0, _gate_q_1, _gate_q_2 {" << std::endl;
                qasm3 << "  h _gate_q_2;" << std::endl;
                qasm3 << "  ccx _gate_q_0, _gate_q_1, _gate_q_2;" << std::endl;
                qasm3 << "  h _gate_q_2;" << std::endl;
                qasm3 << "}" << std::endl;
                break;
            case QkGate_RXX:
                qasm3 << "gate rxx(p0) _gate_q_0, _gate_q_1 {" << std::endl;
                qasm3 << "  h _gate_q_0;" << std::endl;
                qasm3 << "  h _gate_q_1;" << std::endl;
                qasm3 << "  cx _gate_q_0, _gate_q_1;" << std::endl;
                qasm3 << "  rz(p0) _gate_q_1;" << std::endl;
                qasm3 << "  cx _gate_q_0, _gate_q_1;" << std::endl;
                qasm3 << "  h _gate_q_1;" << std::endl;
                qasm3 << "  h _gate_q_0;" << std::endl;
                qasm3 << "}" << std::endl;
                break;
            case QkGate_RZX:
                qasm3 << "gate rzx(p0) _gate_q_0, _gate_q_1 {" << std::endl;
                qasm3 << "  h _gate_q_1;" << std::endl;
                qasm3 << "  cx _gate_q_0, _gate_q_1;" << std::endl;
                qasm3 << "  rz(p0) _gate_q_1;" << std::endl;
                qasm3 << "  cx _gate_q_0, _gate_q_1;" << std::endl;
                qasm3 << "  h _gate_q_1;" << std::endl;
                qasm3 << "}" << std::endl;
                break;
            case QkGate_RZZ:
                qasm3 << "gate rzz(p0) _gate_q_0, _gate_q_1 {" << std::endl;
                qasm3 << "  cx _gate_q_0, _gate_q_1;" << std::endl;
                qasm3 << "  rz(p0) _gate_q_1;" << std::endl;
                qasm3 << "  cx _gate_q_0, _gate_q_1;" << std::endl;
                qasm3 << "}" << std::endl;
                break;
            case QkGate_RCCX:
                qasm3 << "gate rccx _gate_q_0, _gate_q_1, _gate_q_2 {" << std::endl;
                qasm3 << "  h _gate_q_2;" << std::endl;
                qasm3 << "  t _gate_q_2;" << std::endl;
                qasm3 << "  cx _gate_q_1, _gate_q_2;" << std::endl;
                qasm3 << "  tdg _gate_q_2;" << std::endl;
                qasm3 << "  cx _gate_q_0, _gate_q_2;" << std::endl;
                qasm3 << "  t _gate_q_2;" << std::endl;
                qasm3 << "  cx _gate_q_1, _gate_q_2;" << std::endl;
                qasm3 << "  tdg _gate_q_2;" << std::endl;
                qasm3 << "  h _gate_q_2;" << std::endl;
                qasm3 << "}" << std::endl;
                break;
            case QkGate_C3X:
                qasm3 << "gate mcx _gate_q_0, _gate_q_1, _gate_q_2, _gate_q_3 {"
                      << std::endl;
                qasm3 << "  h _gate_q_3;" << std::endl;
                qasm3 << "  p(pi/8) _gate_q_0;" << std::endl;
                qasm3 << "  p(pi/8) _gate_q_1;" << std::endl;
                qasm3 << "  p(pi/8) _gate_q_2;" << std::endl;
                qasm3 << "  p(pi/8) _gate_q_3;" << std::endl;
                qasm3 << "  cx _gate_q_0, _gate_q_1;" << std::endl;
                qasm3 << "  p(-pi/8) _gate_q_1;" << std::endl;
                qasm3 << "  cx _gate_q_0, _gate_q_1;" << std::endl;
                qasm3 << "  cx _gate_q_1, _gate_q_2;" << std::endl;
                qasm3 << "  p(-pi/8) _gate_q_2;" << std::endl;
                qasm3 << "  cx _gate_q_0, _gate_q_2;" << std::endl;
                qasm3 << "  p(pi/8) _gate_q_2;" << std::endl;
                qasm3 << "  cx _gate_q_1, _gate_q_2;" << std::endl;
                qasm3 << "  p(-pi/8) _gate_q_2;" << std::endl;
                qasm3 << "  cx _gate_q_0, _gate_q_2;" << std::endl;
                qasm3 << "  cx _gate_q_2, _gate_q_3;" << std::endl;
                qasm3 << "  p(-pi/8) _gate_q_3;" << std::endl;
                qasm3 << "  cx _gate_q_1, _gate_q_3;" << std::endl;
                qasm3 << "  p(pi/8) _gate_q_3;" << std::endl;
                qasm3 << "  cx _gate_q_2, _gate_q_3;" << std::endl;
                qasm3 << "  p(-pi/8) _gate_q_3;" << std::endl;
                qasm3 << "  cx _gate_q_0, _gate_q_3;" << std::endl;
                qasm3 << "  p(pi/8) _gate_q_3;" << std::endl;
                qasm3 << "  cx _gate_q_2, _gate_q_3;" << std::endl;
                qasm3 << "  p(-pi/8) _gate_q_3;" << std::endl;
                qasm3 << "  cx _gate_q_1, _gate_q_3;" << std::endl;
                qasm3 << "  p(pi/8) _gate_q_3;" << std::endl;
                qasm3 << "  cx _gate_q_2, _gate_q_3;" << std::endl;
                qasm3 << "  p(-pi/8) _gate_q_3;" << std::endl;
                qasm3 << "  cx _gate_q_0, _gate_q_3;" << std::endl;
                qasm3 << "  h _gate_q_3;" << std::endl;
                qasm3 << "}" << std::endl;
                break;
            case QkGate_C3SX:
                qasm3 << "gate c3sx _gate_q_0, _gate_q_1, _gate_q_2, _gate_q_3 {"
                      << std::endl;
                qasm3 << "  h _gate_q_3;" << std::endl;
                qasm3 << "  cp(pi/8) _gate_q_0, _gate_q_3;" << std::endl;
                qasm3 << "  h _gate_q_3;" << std::endl;
                qasm3 << "  cx _gate_q_0, _gate_q_1;" << std::endl;
                qasm3 << "  h _gate_q_3;" << std::endl;
                qasm3 << "  cp(-pi/8) _gate_q_1, _gate_q_3;" << std::endl;
                qasm3 << "  h _gate_q_3;" << std::endl;
                qasm3 << "  cx _gate_q_0, _gate_q_1;" << std::endl;
                qasm3 << "  h _gate_q_3;" << std::endl;
                qasm3 << "  cp(pi/8) _gate_q_1, _gate_q_3;" << std::endl;
                qasm3 << "  h _gate_q_3;" << std::endl;
                qasm3 << "  cx _gate_q_1, _gate_q_2;" << std::endl;
                qasm3 << "  h _gate_q_3;" << std::endl;
                qasm3 << "  cp(-pi/8) _gate_q_2, _gate_q_3;" << std::endl;
                qasm3 << "  h _gate_q_3;" << std::endl;
                qasm3 << "  cx _gate_q_0, _gate_q_2;" << std::endl;
                qasm3 << "  h _gate_q_3;" << std::endl;
                qasm3 << "  cp(pi/8) _gate_q_2, _gate_q_3;" << std::endl;
                qasm3 << "  h _gate_q_3;" << std::endl;
                qasm3 << "  cx _gate_q_1, _gate_q_2;" << std::endl;
                qasm3 << "  h _gate_q_3;" << std::endl;
                qasm3 << "  cp(-pi/8) _gate_q_2, _gate_q_3;" << std::endl;
                qasm3 << "  h _gate_q_3;" << std::endl;
                qasm3 << "  cx _gate_q_0, _gate_q_2;" << std::endl;
                qasm3 << "  h _gate_q_3;" << std::endl;
                qasm3 << "  cp(pi/8) _gate_q_2, _gate_q_3;" << std::endl;
                qasm3 << "  h _gate_q_3;" << std::endl;
                qasm3 << "}" << std::endl;
                break;
            case QkGate_RC3X:
                qasm3 << "gate rcccx _gate_q_0, _gate_q_1, _gate_q_2, _gate_q_3 {"
                      << std::endl;
                qasm3 << "  h _gate_q_3;" << std::endl;
                qasm3 << "  t _gate_q_3;" << std::endl;
                qasm3 << "  cx _gate_q_2, _gate_q_3;" << std::endl;
                qasm3 << "  tdg _gate_q_3;" << std::endl;
                qasm3 << "  h _gate_q_3;" << std::endl;
                qasm3 << "  cx _gate_q_0, _gate_q_3;" << std::endl;
                qasm3 << "  t _gate_q_3;" << std::endl;
                qasm3 << "  cx _gate_q_1, _gate_q_3;" << std::endl;
                qasm3 << "  tdg _gate_q_3;" << std::endl;
                qasm3 << "  cx _gate_q_0, _gate_q_3;" << std::endl;
                qasm3 << "  t _gate_q_3;" << std::endl;
                qasm3 << "  cx _gate_q_1, _gate_q_3;" << std::endl;
                qasm3 << "  tdg _gate_q_3;" << std::endl;
                qasm3 << "  h _gate_q_3;" << std::endl;
                qasm3 << "  t _gate_q_3;" << std::endl;
                qasm3 << "  cx _gate_q_2, _gate_q_3;" << std::endl;
                qasm3 << "  tdg _gate_q_3;" << std::endl;
                qasm3 << "  h _gate_q_3;" << std::endl;
                qasm3 << "}" << std::endl;
                break;
            case QkGate_CU1:
                qasm3 << "gate cu1(p0) _gate_q_0, _gate_q_1 {" << std::endl;
                qasm3 << "  cp(p0) _gate_q_0 _gate_q_1;" << std::endl;
                qasm3 << "}" << std::endl;
                break;
            case QkGate_CU3:
                qasm3 << "gate cu3(p0, p1, p2) _gate_q_0, _gate_q_1 {" << std::endl;
                qasm3 << "  cu(p0, p1, p2, 0) _gate_q_0 _gate_q_1;" << std::endl;
                qasm3 << "}" << std::endl;
                break;
            default:
                break;
            }
        }
    }
    qk_opcounts_clear(&opcounts);

    // save ops
    uint_t nops;
    nops = qk_circuit_num_instructions(rust_circ.get());

    // Declare registers
    // After transpilation, qubit registers will be mapped to physical registers,
    // so we need to combined them in a single quantum register "q";
    const std::string qreg_name = "q";
    qasm3 << "qubit[" << circ.num_qubits() << "] " << qreg_name << ";"
          << std::endl;
    for (const auto &creg : circ.cregs)
    {
        qasm3 << "bit[" << creg.size() << "] " << creg.name() << ";" << std::endl;
    }

    auto recover_reg_data =
        [this](uint_t index) -> std::pair<std::string, uint_t>
    {
        auto it = std::upper_bound(circ.cregs.begin(), circ.cregs.end(), index,
                                   [](uint_t v, const ClassicalRegister &reg)
                                   {
                                       return v < reg.base_index();
                                   });
        assert(it != circ.cregs.begin());
        it = std::prev(it);
        return std::make_pair(it->name(), index - it->base_index());
    };

    for (uint_t i = 0; i < nops; i++)
    {
        QkCircuitInstruction *op = new QkCircuitInstruction;
        qk_circuit_get_instruction(rust_circ.get(), i, op);
        if (op->num_clbits > 0)
        {
            if (op->circ.num_qubits == op->num_clbits)
            {
                for (uint_t j = 0; j < op->circ.num_qubits; j++)
                {
                    const auto creg_data = recover_reg_data(op->clbits[j]);
                    qasm3 << creg_data.first << "[" << creg_data.second
                          << "] = " << op->name << " " << qreg_name << "["
                          << op->qubits[j] << "];" << std::endl;
                }
            }
        }
        else
        {
            if (strcmp(op->name, "u") == 0)
            {
                qasm3 << "U";
            }
            else
            {
                qasm3 << op->name;
            }
            if (op->num_params > 0)
            {
                qasm3 << "(";
                for (uint_t j = 0; j < op->num_params; j++)
                {
                    char *param = qk_param_str(op->params[j]);
                    qasm3 << param;
                    qk_str_free(param);
                    if (j != op->num_params - 1)
                        qasm3 << ", ";
                }
                qasm3 << ")";
            }
            if (op->circ.num_qubits > 0)
            {
                qasm3 << " ";
                for (uint_t j = 0; j < op->circ.num_qubits; j++)
                {
                    qasm3 << qreg_name << "[" << op->qubits[j] << "]";
                    if (j != op->circ.num_qubits - 1)
                        qasm3 << ", ";
                }
            }
            qasm3 << ";" << std::endl;
        }
        qk_circuit_instruction_clear(op);
    }

    return qasm3.str();
}
#endif