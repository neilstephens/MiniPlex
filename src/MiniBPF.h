/*	MiniPlex
 *
 *	Copyright (c) 2023: Neil Stephens
 *
 *	Licensed under the Apache License, Version 2.0 (the "License");
 *	you may not use this file except in compliance with the License.
 *	You may obtain a copy of the License at
 *
 *		http://www.apache.org/licenses/LICENSE-2.0
 *
 *	Unless required by applicable law or agreed to in writing, software
 *	distributed under the License is distributed on an "AS IS" BASIS,
 *	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *	See the License for the specific language governing permissions and
 *	limitations under the License.
 */

/*
 * Minimal subset of eBPF machine code support for basic buffer parsing
 *
 *   You should be able to write some eBPF assembly using only the supported
 *   operations, Assemble it into bytecode using uBPF etc, and save the raw byte code to a file
 */

#ifndef MINIBPF_H
#define MINIBPF_H

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <fstream>

namespace MiniBPF {

// eBPF instruction format (64-bit) - matches actual eBPF
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
struct Instruction {
		uint8_t opcode;
		uint8_t dst_reg : 4;
		uint8_t src_reg : 4;
		int16_t offset;
		int32_t imm;
}
#ifdef __GNUC__
__attribute__((packed))
#endif
;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

static_assert(sizeof(Instruction) == 8, "Instruction must be 8 bytes");

// eBPF opcodes - matching Linux kernel eBPF
/*   Store not supported		*/
/*   Mode modifiers not supported	*/
/*   Call not supported			*/
/*   Source modifiers not supported	*/

namespace OpCode {
// Instruction classes (bits 0-2)
constexpr uint8_t BPF_LD = 0x00;
constexpr uint8_t BPF_LDX = 0x01;
constexpr uint8_t BPF_ALU = 0x04;
constexpr uint8_t BPF_JMP = 0x05;
constexpr uint8_t BPF_JMP32 = 0x06;
constexpr uint8_t BPF_ALU64 = 0x07;

// Size modifiers (bits 3-4)
constexpr uint8_t BPF_W = 0x00;  // 32-bit
constexpr uint8_t BPF_H = 0x08;  // 16-bit
constexpr uint8_t BPF_B = 0x10;  // 8-bit
constexpr uint8_t BPF_DW = 0x18; // 64-bit

// ALU/ALU64 operations (bits 4-7)
constexpr uint8_t BPF_ADD = 0x00;
constexpr uint8_t BPF_SUB = 0x10;
constexpr uint8_t BPF_MUL = 0x20;
constexpr uint8_t BPF_DIV = 0x30;
constexpr uint8_t BPF_OR = 0x40;
constexpr uint8_t BPF_AND = 0x50;
constexpr uint8_t BPF_LSH = 0x60;
constexpr uint8_t BPF_RSH = 0x70;
constexpr uint8_t BPF_NEG = 0x80;
constexpr uint8_t BPF_MOD = 0x90;
constexpr uint8_t BPF_XOR = 0xa0;
constexpr uint8_t BPF_MOV = 0xb0;
constexpr uint8_t BPF_ARSH = 0xc0;

// Jump operations (bits 4-7)
constexpr uint8_t BPF_JA = 0x00;
constexpr uint8_t BPF_JEQ = 0x10;
constexpr uint8_t BPF_JGT = 0x20;
constexpr uint8_t BPF_JGE = 0x30;
constexpr uint8_t BPF_JSET = 0x40;
constexpr uint8_t BPF_JNE = 0x50;
constexpr uint8_t BPF_JSGT = 0x60;
constexpr uint8_t BPF_JSGE = 0x70;
constexpr uint8_t BPF_EXIT = 0x90;
constexpr uint8_t BPF_JLT = 0xa0;
constexpr uint8_t BPF_JLE = 0xb0;
constexpr uint8_t BPF_JSLT = 0xc0;
constexpr uint8_t BPF_JSLE = 0xd0;
} // namespace OpCode

class VM
{
public:
	VM() :
		regs{},
		memory(nullptr),
		memory_size(0)
	{}

	// Set register value (r0-r10)
	void set_register(int reg, uint64_t value)
	{
		if (reg < 0 || reg >= 11)
			throw std::runtime_error("Invalid register number");
		regs[reg] = value;
	}

	// Get register value (r0-r10)
	uint64_t get_register(int reg) const
	{
		if (reg < 0 || reg >= 11)
			throw std::runtime_error("Invalid register number");
		return regs[reg];
	}

	// Set memory context for load operations
	void set_memory_context(const void* mem, size_t size)
	{
		memory = static_cast<const uint8_t *>(mem);
		memory_size = size;
	}

	void load(const std::string& filename)
	{
		std::ifstream fin(filename,std::ios::binary|std::ios::ate);
		if (fin.fail())
			throw std::invalid_argument("Failed to open eBPF bytecode file: "+filename);
		size_t size = fin.tellg();
		if(size > 64L*1024)
			throw std::invalid_argument("eBPF bytecode file too large (max 64kB): "+filename);
		fin.seekg(0,std::ios::beg);
		std::vector<uint8_t> bytecode(size);
		fin.read(reinterpret_cast<char *>(bytecode.data()), size);
		load(bytecode.data(),size);
	}

	// Load program bytecode
	void load(const void *bytecode, size_t size_in_bytes)
	{
		if (size_in_bytes % sizeof(Instruction) != 0)
			throw std::invalid_argument("Bytecode size must be multiples of instruction size (8 bytes)");

		const Instruction *instructions = static_cast<const Instruction *>(bytecode);
		size_t count = size_in_bytes / sizeof(Instruction);

		program.resize(count);
		std::copy(instructions, instructions + count, program.begin());
	}

	// Validate all the instructions in the program
	bool validate(std::ostream &err)
	{
		auto memory_bak = memory;
		auto memory_size_bak = memory_size;
		uint8_t temp;
		memory = &temp;
		memory_size = 1;

		bool valid = true;
		for (size_t pc = 0; pc < program.size(); pc++)
		{
			try
			{

				auto tmp = pc;
				execute_instruction(program[pc], tmp);
			}
			catch (const std::invalid_argument &e)
			{
				valid = false;
				err << std::string("MiniBPF Exception: ") + e.what() << std::string("\n");
			}
			catch (const std::runtime_error &)
			{}
		}
		memory = memory_bak;
		memory_size = memory_size_bak;
		return valid;
	}

	// Execute program
	void execute(size_t max_instructions = 10000)
	{
		size_t pc = 0;
		size_t instruction_count = 0;
		halted = false;

		while (pc < program.size() && !halted)
		{
			if (++instruction_count > max_instructions)
				throw std::runtime_error("Maximum instruction count exceeded");

			const auto &inst = program[pc];
			execute_instruction(inst, pc);
		}
	}

private:
	uint64_t regs[11]; // r0-r10
	const uint8_t* memory;
	size_t memory_size;
	std::vector<Instruction> program;
	bool halted;

	void execute_instruction(const Instruction& inst, size_t& pc)
	{
		uint8_t cls = inst.opcode & 0x07;
		uint8_t op = inst.opcode & 0xf0;
		uint8_t src_mode = inst.opcode & 0x08;

		switch (cls) {
			case OpCode::BPF_ALU64:
				execute_alu64(inst, op, src_mode);
				pc++;
				break;

			case OpCode::BPF_ALU:
				execute_alu32(inst, op, src_mode);
				pc++;
				break;

			case OpCode::BPF_LDX:
				execute_load(inst);
				pc++;
				break;

			case OpCode::BPF_JMP:
				execute_jump(inst, op, src_mode, pc);
				break;

			case OpCode::BPF_JMP32:
				execute_jump32(inst, op, src_mode, pc);
				break;

			default:
				throw std::invalid_argument("Unsupported instruction class");
		}
	}

	void execute_alu64(const Instruction& inst, uint8_t op, uint8_t src_mode)
	{
		uint64_t src_val = src_mode ? regs[inst.src_reg]
						    : static_cast<uint64_t>(static_cast<int64_t>(inst.imm));
		uint64_t& dst = regs[inst.dst_reg];

		switch (op) {
			case OpCode::BPF_ADD:
				dst += src_val;
				break;
			case OpCode::BPF_SUB:
				dst -= src_val;
				break;
			case OpCode::BPF_MUL:
				dst *= src_val;
				break;
			case OpCode::BPF_DIV:
				dst = src_val ? dst / src_val : 0;
				break;
			case OpCode::BPF_OR:
				dst |= src_val;
				break;
			case OpCode::BPF_AND:
				dst &= src_val;
				break;
			case OpCode::BPF_LSH:
				dst <<= src_val;
				break;
			case OpCode::BPF_RSH:
				dst >>= src_val;
				break;
			case OpCode::BPF_NEG:
				dst = -dst;
				break;
			case OpCode::BPF_MOD:
				dst = src_val ? dst % src_val : dst;
				break;
			case OpCode::BPF_XOR:
				dst ^= src_val;
				break;
			case OpCode::BPF_MOV:
				dst = src_val;
				break;
			case OpCode::BPF_ARSH:
				dst = static_cast<uint64_t>(static_cast<int64_t>(dst) >> src_val);
				break;
			default:
				throw std::invalid_argument("Unknown ALU64 operation");
		}
	}

	void execute_alu32(const Instruction& inst, uint8_t op, uint8_t src_mode)
	{
		uint32_t src_val = src_mode ? static_cast<uint32_t>(regs[inst.src_reg])
						    : static_cast<uint32_t>(inst.imm);
		uint32_t dst = static_cast<uint32_t>(regs[inst.dst_reg]);

		switch (op) {
			case OpCode::BPF_ADD:
				dst += src_val;
				break;
			case OpCode::BPF_SUB:
				dst -= src_val;
				break;
			case OpCode::BPF_MUL:
				dst *= src_val;
				break;
			case OpCode::BPF_DIV:
				dst = src_val ? dst / src_val : 0;
				break;
			case OpCode::BPF_OR:
				dst |= src_val;
				break;
			case OpCode::BPF_AND:
				dst &= src_val;
				break;
			case OpCode::BPF_LSH:
				dst <<= src_val;
				break;
			case OpCode::BPF_RSH:
				dst >>= src_val;
				break;
			case OpCode::BPF_NEG:
				dst = -dst;
				break;
			case OpCode::BPF_MOD:
				dst = src_val ? dst % src_val : dst;
				break;
			case OpCode::BPF_XOR:
				dst ^= src_val;
				break;
			case OpCode::BPF_MOV:
				dst = src_val;
				break;
			case OpCode::BPF_ARSH:
				dst = static_cast<uint32_t>(static_cast<int32_t>(dst) >> src_val);
				break;
			default:
				throw std::invalid_argument("Unknown ALU32 operation");
		}

		// Zero-extend to 64 bits
		regs[inst.dst_reg] = static_cast<uint64_t>(dst);
	}

	void execute_load(const Instruction& inst)
	{
		if (!memory)
			throw std::invalid_argument("No memory context set for load operation");

		size_t addr = regs[inst.src_reg] + inst.offset;
		uint8_t size_mode = inst.opcode & 0x18;

		switch (size_mode)
		{
			case OpCode::BPF_B: // 8-bit
				if (addr >= memory_size)
					throw std::runtime_error("Load out of bounds");
				regs[inst.dst_reg] = memory[addr];
				break;

			case OpCode::BPF_H: // 16-bit
				if (addr + 1 >= memory_size)
					throw std::runtime_error("Load out of bounds");
				regs[inst.dst_reg] = *reinterpret_cast<const uint16_t *>(&memory[addr]);
				break;

			case OpCode::BPF_W: // 32-bit
				if (addr + 3 >= memory_size)
					throw std::runtime_error("Load out of bounds");
				regs[inst.dst_reg] =*reinterpret_cast<const uint32_t *>(&memory[addr]);
				break;

			case OpCode::BPF_DW: // 64-bit
				if (addr + 7 >= memory_size)
					throw std::runtime_error("Load out of bounds");
				regs[inst.dst_reg] =*reinterpret_cast<const uint64_t *>(&memory[addr]);
				break;
		}
	}

	void execute_jump(const Instruction& inst, uint8_t op, uint8_t src_mode, size_t& pc)
	{
		if (op == OpCode::BPF_EXIT)
		{
			halted = true;
			return;
		}

		if (op == OpCode::BPF_JA)
		{
			pc = static_cast<size_t>(static_cast<int64_t>(pc) + inst.offset + 1);
			return;
		}

		uint64_t dst = regs[inst.dst_reg];
		uint64_t src = src_mode ? regs[inst.src_reg]
						: static_cast<uint64_t>(static_cast<int64_t>(inst.imm));
		bool taken = false;

		switch (op)
		{
			case OpCode::BPF_JEQ:
				taken = (dst == src);
				break;
			case OpCode::BPF_JGT:
				taken = (dst > src);
				break;
			case OpCode::BPF_JGE:
				taken = (dst >= src);
				break;
			case OpCode::BPF_JLT:
				taken = (dst < src);
				break;
			case OpCode::BPF_JLE:
				taken = (dst <= src);
				break;
			case OpCode::BPF_JNE:
				taken = (dst != src);
				break;
			case OpCode::BPF_JSGT:
				taken = (static_cast<int64_t>(dst) > static_cast<int64_t>(src));
				break;
			case OpCode::BPF_JSGE:
				taken = (static_cast<int64_t>(dst) >= static_cast<int64_t>(src));
				break;
			case OpCode::BPF_JSLT:
				taken = (static_cast<int64_t>(dst) < static_cast<int64_t>(src));
				break;
			case OpCode::BPF_JSLE:
				taken = (static_cast<int64_t>(dst) <= static_cast<int64_t>(src));
				break;
			case OpCode::BPF_JSET:
				taken = ((dst & src) != 0);
				break;
			default:
				throw std::invalid_argument("Unknown jump operation");
		}

		if (taken)
			pc = static_cast<size_t>(static_cast<int64_t>(pc) + inst.offset + 1);
		else
			pc++;
	}

	void execute_jump32(const Instruction& inst, uint8_t op, uint8_t src_mode, size_t& pc)
	{
		uint32_t dst = static_cast<uint32_t>(regs[inst.dst_reg]);
		uint32_t src = src_mode ? static_cast<uint32_t>(regs[inst.src_reg])
						: static_cast<uint32_t>(inst.imm);
		bool taken = false;

		switch (op) {
			case OpCode::BPF_JEQ:
				taken = (dst == src);
				break;
			case OpCode::BPF_JGT:
				taken = (dst > src);
				break;
			case OpCode::BPF_JGE:
				taken = (dst >= src);
				break;
			case OpCode::BPF_JLT:
				taken = (dst < src);
				break;
			case OpCode::BPF_JLE:
				taken = (dst <= src);
				break;
			case OpCode::BPF_JNE:
				taken = (dst != src);
				break;
			case OpCode::BPF_JSGT:
				taken = (static_cast<int32_t>(dst) > static_cast<int32_t>(src));
				break;
			case OpCode::BPF_JSGE:
				taken = (static_cast<int32_t>(dst) >= static_cast<int32_t>(src));
				break;
			case OpCode::BPF_JSLT:
				taken = (static_cast<int32_t>(dst) < static_cast<int32_t>(src));
				break;
			case OpCode::BPF_JSLE:
				taken = (static_cast<int32_t>(dst) <= static_cast<int32_t>(src));
				break;
			case OpCode::BPF_JSET:
				taken = ((dst & src) != 0);
				break;
			default:
				throw std::invalid_argument("Unknown jump32 operation");
		}

		if (taken)
			pc = static_cast<size_t>(static_cast<int64_t>(pc) + inst.offset + 1);
		else
			pc++;
	}
};

} // namespace MiniBPF

#endif // MINIBPF_H
