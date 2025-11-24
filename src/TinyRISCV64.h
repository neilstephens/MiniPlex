/*
 * TinyRISCV64 - RV64IM Virtual Machine
 *
 * https://github.com/neilstephens/TinyRISCV64
 *
 * This is a derivative work based on tinyriscv by inixyz (https://github.com/inixyz/tinyriscv)
 * The core instruction processing logic was ported from C to C++,
 * converted from handling RV32IM to RV64IM, and
 * transplanted into this new class.
 *
 * MIT License
 *
 * Original work Copyright (c) 2023 Alexandru-Florin Ene
 * Modified work Copyright (c) 2025 Neil Stephens
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef TINYRISCV64_H
#define TINYRISCV64_H

#include <cstdint>
#include <vector>
#include <span>
#include <stdexcept>
#include <fstream>
#include <cstring>
#include <array>

namespace TinyRISCV64
{

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

class VM
{
private:
	u64 pc;                    // Program counter
	std::vector<u8> program;   // Program memory
	std::array<u64,32> x{};    // Registers x0-x31
	std::vector<u8> stack;     // Stack memory
	std::span<u8> data;        // Data memory
	bool halted{false};        // Program exited

	// Virtual addressing:
	static constexpr
	u64 p_beg = 0;   // Program mem begin
	u64 p_end;       // Program mem end
							/* 64 overflow detection addresses */
	u64 d_beg;       // Data mem begin
	u64 d_end;       // Data mem end
							/* 64 overflow detection addresses */
	u64 s_beg;       // Stack mem begin
	u64 s_end;       // Stack mem end

public:
	VM(const size_t stack_size = 4096): stack(stack_size) { reset(); }

	// Load bytecode from file and return the starting virtual addr
	//   resets state and invalidates previous virtual addrs
	u64 program_load(const std::string& prog_filename)
	{
		program = load_program(prog_filename);
		reset();
		return p_beg;
	}

	// Copy bytecode and return the starting virtual addr
	//   resets state and invalidates previous virtual addrs
	u64 program_load(const u8* const prog, size_t prog_size)
	{
		program = load_program(prog,prog_size);
		reset();
		return p_beg;
	}

	// Map virtual addresses to the referenced data
	//   resets state and invalidates previous virtual addrs
	u64 map_data_mem(u8* const mem, const size_t mem_size)
	{
		data = {mem,mem_size};
		reset();
		return d_beg;
	}

	// Set register value (x0-x31, x0 is always 0)
	void register_set(const size_t reg, const u64 value)
	{
		if (reg < 0 || reg >= 32)
			throw std::invalid_argument("Invalid register number");
		if (reg != 0) x[reg] = value;
	}

	// Get register value
	u64 register_get(const size_t reg) const
	{
		if (reg < 0 || reg >= 32)
			throw std::invalid_argument("Invalid register number");
		return x[reg];
	}

	// Push a value onto the stack and return the virtual address (stack pointer)
	template<typename T>
	u64 stack_push(const T& val)
	{
		x[2] -= sizeof(T);
		mem_store(x[2],val);
		return x[2];
	}

	template<typename T>
	T stack_pop()
	{
		x[2] += sizeof(T);
		return mem_load<T>(x[2]-sizeof(T));
	}

	template<typename T>
	T stack_peek()
	{
		return mem_load<T>(x[2]);
	}

	// Execute program
	void execute_program(const u64 entry_point = p_beg, const size_t max_instructions = 100000)
	{
		pc = entry_point;
		halted = false;
		size_t count = 0;

		while (!halted && pc+3 < program.size())
		{
			if (++count > max_instructions)
				throw std::runtime_error("Maximum instruction count exceeded");
			execute_instruction();
		}
	}

	void reset()
	{
		for(auto& xn : x) xn=0;
		//x1 - return address (ra)
		x[1] = program.size();
		//x2 - stack pointer (sp)
		x[2] = program.size()+64+data.size()+64+stack.size();
		//x8 - frame pointer (s0 / fp)
		x[8] = x[2];

		p_end = program.size();
		/* 64 overflow detection addresses */
		d_beg = program.size()+64;
		d_end = program.size()+64+data.size();
		/* 64 overflow detection addresses */
		s_beg = program.size()+64+data.size()+64;
		s_end = program.size()+64+data.size()+64+stack.size();
	}

private:

	// Load program from file
	static std::vector<u8> load_program(const std::string& filename)
	{
		std::ifstream fin(filename, std::ios::binary | std::ios::ate);
		if (!fin)
			throw std::invalid_argument("Failed to open program file: " + filename);

		size_t size = fin.tellg();
		if (size > 1024L * 1024)
			throw std::invalid_argument("Program too large (max 1MB)");

		fin.seekg(0, std::ios::beg);
		std::vector<u8> prog(size);
		fin.read(reinterpret_cast<char*>(prog.data()), size);
		return prog;
	}

	// Load program directly
	static std::vector<u8> load_program(const u8* const prog, const size_t size)
	{
		if (size > 1024L * 1024)
			throw std::invalid_argument("Program too large (max 1MB)");

		std::vector<u8> progr(size);
		std::memcpy(progr.data(), prog, size);
		return progr;
	}

	void execute_instruction()
	{
		u32 inst; memcpy(&inst,&program[pc],4);
		pc += 4;

		// Decode
		const u8 opcode = inst & 0x7f;
		const u8 funct3 = (inst >> 12) & 0x7;
		const u8 funct7 = (inst >> 25) & 0x7f;
		const u8 rd = (inst >> 7) & 0x1f;
		const u8 rs1 = (inst >> 15) & 0x1f;
		const u8 rs2 = (inst >> 20) & 0x1f;

		const i64 imm_i = static_cast<i64>(static_cast<i32>(inst) >> 20);
		const i64 imm_s = (imm_i & ~0x1fLL) | rd;
		const i64 imm_b = ((static_cast<i64>(static_cast<i32>(inst & 0x80000000)) >> 19) |
					 ((inst & 0x80) << 4) | ((inst >> 20) & 0x7e0) | ((inst >> 7) & 0x1e));
		const i64 imm_j = ((static_cast<i64>(static_cast<i32>(inst & 0x80000000)) >> 11) |
					 (inst & 0xff000) | ((inst >> 9) & 0x800) | ((inst >> 20) & 0x7fe));
		const u64 imm_u = inst & 0xfffff000;

		// Execute
		x[0] = 0; // Ensure x0 stays zero

		switch(opcode)
		{
			case 0x37: x[rd] = imm_u; break;               // LUI
			case 0x17: x[rd] = (pc - 4) + imm_u; break;    // AUIPC
			case 0x6f: x[rd] = pc; pc += imm_j - 4; break; // JAL
			case 0x67:                                     // JALR
			{
				const u64 target = (x[rs1] + imm_i) & ~1ULL;
				x[rd] = pc;
				pc = target;
				break;
			}
			case 0x63: exec_branch(funct3, rs1, rs2, imm_b); break;                  // Branch
			case 0x03: exec_load(funct3, rd, rs1, imm_i); break;                     // Load
			case 0x23: exec_store(funct3, rs1, rs2, imm_s); break;                   // Store
			case 0x13: exec_alu_imm(funct3, rd, rs1, imm_i); break;                  // ALU immediate
			case 0x1b: exec_alu_imm32(funct3, rd, rs1, imm_i); break;                // ALU immediate 32-bit
			case 0x33: exec_alu_reg(funct3, funct7, rd, rs1, rs2); break;            // ALU register
			case 0x3b: exec_alu_reg32(funct3, funct7, rd, rs1, rs2); break;          // ALU register 32-bit
			case 0x0f: break;                                                        // FENCE (nop)
			case 0x73:                                                               // SYSTEM
				if (inst == 0x00100073) halted = true;                             // EBREAK
				break;
			default: throw std::invalid_argument("Unknown opcode");
		}
	}

	// Memory access helpers
	template<typename T>
	u8* mem_ptr(u64 addr)
	{
		if (addr > 0xFFFFFFFFFFFFFFF0ULL) //guard against wrap-around
			throw std::runtime_error("Memory access out of bounds");

		const u64 addr_max = addr + sizeof(T) - 1;

		if(addr_max < p_end)
			return program.data() + addr;
		if(addr >= d_beg && addr_max < d_end)
			return data.data() + addr - d_beg;
		if(addr >= s_beg && addr_max < s_end)
			return stack.data() + addr - s_beg;

		throw std::runtime_error("Memory access out of bounds");
	}

	template<typename T>
	T mem_load(u64 addr)
	{
		T value;
		memcpy(&value, mem_ptr<T>(addr), sizeof(T));
		return value;
	}

	template<typename T>
	void mem_store(u64 addr, T value)
	{
		memcpy(mem_ptr<T>(addr), &value, sizeof(T));
	}

	// Instruction execution helpers
	inline void exec_branch(u8 funct3, u8 rs1, u8 rs2, i64 imm)
	{
		bool taken = false;
		switch(funct3)
		{
			case 0: taken = (x[rs1] == x[rs2]); break;                                     // BEQ
			case 1: taken = (x[rs1] != x[rs2]); break;                                     // BNE
			case 4: taken = (static_cast<i64>(x[rs1]) < static_cast<i64>(x[rs2])); break;  // BLT
			case 5: taken = (static_cast<i64>(x[rs1]) >= static_cast<i64>(x[rs2])); break; // BGE
			case 6: taken = (x[rs1] < x[rs2]); break;                                      // BLTU
			case 7: taken = (x[rs1] >= x[rs2]); break;                                     // BGEU
			default: throw std::invalid_argument("Unknown branch operation");
		}
		if (taken) pc += imm - 4;
	}

	inline void exec_load(u8 funct3, u8 rd, u8 rs1, i64 imm)
	{
		const u64 addr = x[rs1] + imm;
		switch(funct3)
		{
			case 0: x[rd] = static_cast<i64>(mem_load<i8>(addr)); break;  // LB
			case 1: x[rd] = static_cast<i64>(mem_load<i16>(addr)); break; // LH
			case 2: x[rd] = static_cast<i64>(mem_load<i32>(addr)); break; // LW
			case 3: x[rd] = mem_load<u64>(addr); break;                   // LD
			case 4: x[rd] = mem_load<u8>(addr); break;                    // LBU
			case 5: x[rd] = mem_load<u16>(addr); break;                   // LHU
			case 6: x[rd] = mem_load<u32>(addr); break;                   // LWU
			default: throw std::invalid_argument("Unknown load operation");
		}
	}

	inline void exec_store(u8 funct3, u8 rs1, u8 rs2, i64 imm)
	{
		const u64 addr = x[rs1] + imm;
		switch(funct3)
		{
			case 0: mem_store<u8>(addr, x[rs2]); break;  // SB
			case 1: mem_store<u16>(addr, x[rs2]); break; // SH
			case 2: mem_store<u32>(addr, x[rs2]); break; // SW
			case 3: mem_store<u64>(addr, x[rs2]); break; // SD
			default: throw std::invalid_argument("Unknown store operation");
		}
	}

	inline void exec_alu_imm(u8 funct3, u8 rd, u8 rs1, i64 imm)
	{
		switch(funct3)
		{
			case 0: x[rd] = x[rs1] + imm; break;                   // ADDI
			case 1: x[rd] = x[rs1] << imm; break;                  // SLLI
			case 2: x[rd] = static_cast<i64>(x[rs1]) < imm; break; // SLTI
			case 3: x[rd] = x[rs1] < static_cast<u64>(imm); break; // SLTIU
			case 4: x[rd] = x[rs1] ^ imm; break;                   // XORI
			case 5:
				if (!(imm&0x400)) x[rd] = x[rs1] >> imm;                               // SRLI
				else x[rd] = static_cast<u64>(static_cast<i64>(x[rs1]) >> (imm&0x3f)); // SRAI
				break;
			case 6: x[rd] = x[rs1] | imm; break; // ORI
			case 7: x[rd] = x[rs1] & imm; break; // ANDI
			default: throw std::invalid_argument("Unknown alu_imm operation");
		}
	}

	inline void exec_alu_imm32(u8 funct3, u8 rd, u8 rs1, i32 imm)
	{
		u32 result;
		switch(funct3)
		{
			case 0: result = static_cast<u32>(x[rs1]) + imm; break;    // ADDIW
			case 1: result = static_cast<u32>(x[rs1]) << imm; break;   // SLLIW
			case 5:
				if (!(imm&0x400))
					result = static_cast<u32>(x[rs1]) >> imm;      // SRLIW
				else
					result = static_cast<u32>(static_cast<i32>(x[rs1]) >> (imm&0x1f)); // SRAIW
				break;
			default: throw std::invalid_argument("Unknown alu_imm32 operation");
		}
		x[rd] = static_cast<i64>(static_cast<i32>(result)); // Sign-extend
	}

	inline void exec_alu_reg(u8 funct3, u8 funct7, u8 rd, u8 rs1, u8 rs2)
	{
		const auto op = funct7 << 3 | funct3;
		switch(op)
		{
			case 0x000: x[rd] = x[rs1] + x[rs2]; break;                                               // ADD
			case 0x100: x[rd] = x[rs1] - x[rs2]; break;                                               // SUB
			case 0x001: x[rd] = x[rs1] << (x[rs2] & 0x3f); break;                                     // SLL
			case 0x002: x[rd] = static_cast<i64>(x[rs1]) < static_cast<i64>(x[rs2]); break;           // SLT
			case 0x003: x[rd] = x[rs1] < x[rs2]; break;                                               // SLTU
			case 0x004: x[rd] = x[rs1] ^ x[rs2]; break;                                               // XOR
			case 0x005: x[rd] = x[rs1] >> (x[rs2] & 0x3f); break;                                     // SRL
			case 0x105: x[rd] = static_cast<u64>(static_cast<i64>(x[rs1]) >> (x[rs2] & 0x3f)); break; // SRA
			case 0x006: x[rd] = x[rs1] | x[rs2]; break;                                               // OR
			case 0x007: x[rd] = x[rs1] & x[rs2]; break;                                               // AND

			// M extension
			case 0x008: x[rd] = x[rs1] * x[rs2]; break;                                               // MUL
			case 0x009: x[rd] = mulh(x[rs1],x[rs2]); break;                                           // MULH
			case 0x00a: x[rd] = mulhsu(x[rs1],x[rs2]); break;                                         // MULHSU
			case 0x00b: x[rd] = mulhu(x[rs1],x[rs2]); break;                                          // MULHU
			case 0x00c: // DIV
				if (x[rs2]) x[rd] = (static_cast<i64>(x[rs1]) == INT64_MIN && static_cast<i64>(x[rs2]) == -1)
							  ? static_cast<u64>(INT64_MIN)
							  : static_cast<u64>(static_cast<i64>(x[rs1]) / static_cast<i64>(x[rs2]));
				else x[rd] = 0xFFFFFFFFFFFFFFFFULL;
				break;
			case 0x00d: // DIVU
				if (x[rs2]) x[rd] = x[rs1] / x[rs2];
				else x[rd] = 0xFFFFFFFFFFFFFFFFULL;
				break;
			case 0x00e: // REM
				if (x[rs2]) x[rd] = (static_cast<i64>(x[rs1]) == INT64_MIN && static_cast<i64>(x[rs2]) == -1)
							  ? 0ULL
							  : static_cast<u64>(static_cast<i64>(x[rs1]) % static_cast<i64>(x[rs2]));
				else x[rd] = x[rs1];
				break;
			case 0x00f: // REMU
				if (x[rs2]) x[rd] = x[rs1] % x[rs2];
				else x[rd] = x[rs1];
				break;
			default: throw std::invalid_argument("Unknown alu_reg operation");
		}
	}

	inline void exec_alu_reg32(u8 funct3, u8 funct7, u8 rd, u8 rs1, u8 rs2)
	{
		const auto op = funct7 << 3 | funct3;
		i32 result;
		u32 a = static_cast<u32>(x[rs1]);
		u32 b = static_cast<u32>(x[rs2]);

		switch(op)
		{
			case 0x000: result = a + b; break;                             // ADDW
			case 0x100: result = a - b; break;                             // SUBW
			case 0x001: result = a << (b & 0x1f); break;                   // SLLW
			case 0x005: result = a >> (b & 0x1f); break;                   // SRLW
			case 0x105: result = static_cast<i32>(a) >> (b & 0x1f); break; // SRAW

			// M extension 32-bit
			case 0x008: result = static_cast<i32>(a * b); break; // MULW
			case 0x00c:
				if (b) result = (static_cast<i32>(a) == INT32_MIN && static_cast<i32>(b) == -1)
						    ? static_cast<i32>(INT32_MIN)
						    : static_cast<i32>(a) / static_cast<i32>(b);
				else result = -1;
				break; // DIVW
			case 0x00d:
				if (b) result = a / b;
				else result = -1;
				break; // DIVUW
			case 0x00e:
				if (b) result = static_cast<i32>(a) % static_cast<i32>(b);
				else result = static_cast<i32>(a);
				break; // REMW
			case 0x00f:
				if (b) result = a % b;
				else result = static_cast<i32>(a);
				break; // REMUW
			default: throw std::invalid_argument("Unknown alu_reg32 operation");
		}
		x[rd] = static_cast<i64>(result); // Sign-extend to 64 bits
	}

	// For 128-bit multiplication - TODO: use platform intrinsics (_umul128 on MSVC and __int128 specifically for GCC/Clang)
	#if defined(__SIZEOF_INT128__)
	using i128 = __int128; using u128 = unsigned __int128;
	static inline uint64_t mulh(i64 a, i64 b) { return (static_cast<i128>(a) * static_cast<i128>(b)) >> 64; }
	static inline uint64_t mulhu(u64 a, u64 b) { return (static_cast<u128>(a) * static_cast<u128>(b)) >> 64; }
	static inline uint64_t mulhsu(i64 a, u64 b) { return (static_cast<i128>(a) * static_cast<u128>(b)) >> 64; }
	#else
	//Provide emulation on platforms without __int128
	static inline std::pair<u64,u64> mulu64_128(u64 a, u64 b)
	{
		constexpr u64 TRUNC32 = 0xFFFFFFFFULL;
		const u64 a_lo = a & TRUNC32;
		const u64 a_hi = a >> 32;
		const u64 b_lo = b & TRUNC32;
		const u64 b_hi = b >> 32;

		u64 p0 = a_lo * b_lo;
		u64 p1 = a_lo * b_hi;
		u64 p2 = a_hi * b_lo;
		u64 p3 = a_hi * b_hi;

		u64 mid = (p0 >> 32) + (p1 & TRUNC32) + (p2 & TRUNC32);
		u64 lo = (p0 & TRUNC32) | (mid << 32);
		u64 hi = p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);

		return {hi, lo};
	}
	static inline i64 mulh(i64 a, i64 b)
	{
		bool neg = (a < 0) ^ (b < 0);
		u64 abs_a = (a < 0) ? (0ULL - static_cast<u64>(a)) : static_cast<u64>(a);
		u64 abs_b = (b < 0) ? (0ULL - static_cast<u64>(b)) : static_cast<u64>(b);

		auto [hi, lo] = mulu64_128(abs_a, abs_b);

		if (neg) //2's compliment
		{
			hi = ~hi;
			lo = ~lo;
			lo += 1;
			if (lo == 0) ++hi;
		}
		return static_cast<i64>(hi);
	}
	static inline i64 mulhsu(i64 a, u64 b)
	{
		if(a < 0)
		{
			u64 abs_a = 0ULL - static_cast<u64>(a);

			auto [hi, lo] = mulu64_128(abs_a, b);

			//2's compliment
			hi = ~hi;
			lo = ~lo;
			lo += 1;
			if (lo == 0) ++hi;

			return static_cast<i64>(hi);
		}
		return mulu64_128(static_cast<u64>(a),b).first;
	}
	static inline uint64_t mulhu(u64 a, u64 b){ return mulu64_128(a,b).first; }
	#endif
};

} // namespace TinyRISCV64

#endif // TINYRISCV64_H
