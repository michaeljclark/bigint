#include <map>
#include <deque>
#include <vector>
#include <memory>
#include <string>
#include <iostream>
#include <iomanip>

#include "nat.h"
#include "nat-parser.hh"
#include "nat-compiler.h"
#include "FlexLexer.h"
#include "nat-scanner.h"
#include "nat-target.h"


using namespace nat;


/*-----------.
| constants. |
`-----------*/

const char* nat::op_name[] = {
	"none",
	"const_int",
	"var",
	"setvar",
	"ssareg",
	"phyreg",
	"setreg",
	"imm",
	"mi",
	"li",
	"and",
	"or",
	"xor",
	"seq",
	"sne",
	"slt",
	"sle",
	"sgt",
	"sge",
	"srl",
	"srli",
	"sll",
	"slli",
	"add",
	"addi",
	"sub",
	"mul",
	"div",
	"rem",
	"not",
	"neg",
};


/*-------------------.
| node constructors. |
`-------------------*/

node::node(type typecode, op opcode)
	: typecode(typecode), opcode(opcode) {}

const_int::const_int(std::string r)
	: node(type_const_int, op_const_int),
	  r(std::unique_ptr<Nat>(new Nat(r))) {}

const_int::const_int(Nat &n)
	: node(type_const_int, op_const_int),
	  r(std::unique_ptr<Nat>(new Nat(n))) {}

unaryop::unaryop(op opcode, node *l)
	: node(type_unaryop, opcode),
	  l(std::unique_ptr<node>(l)) {}

binaryop::binaryop(op opcode, node *l, node *r)
	: node(type_binaryop, opcode),
	  l(std::unique_ptr<node>(l)),
	  r(std::unique_ptr<node>(r)) {}

var::var(std::string l)
	: node(type_var, op_var),
	  l(std::unique_ptr<std::string>(new std::string(l))) {}

setvar::setvar(std::string l, node *r)
	: node(type_setvar, op_setvar),
	  l(std::unique_ptr<std::string>(new std::string(l))),
	  r(std::unique_ptr<node>(r)) {}

reg::reg(type typecode, op opcode, size_t l)
	: node(typecode, opcode), l(l) {}

ssareg::ssareg(size_t l)
	: reg(type_ssareg, op_ssareg, l) {}

phyreg::phyreg(size_t l)
	: reg(type_phyreg, op_phyreg, l) {}

setreg::setreg(reg *l, node *r)
	: node(type_setreg, op_setreg),
	  l(std::unique_ptr<reg>(l)),
	  r(std::unique_ptr<node>(r)) {}

imm::imm(int r)
	: node(type_imm, op_imm), r(r) {}

target::machineinst::machineinst()
	: node(type_mi, op_mi) {}


/*-----------------------------------.
| evalulate expressions recursively. |
`-----------------------------------*/

Nat node::eval(compiler *d)
{
	return Nat(0);
}

Nat unaryop::eval(compiler *d)
{
	Nat v = 0;
	switch(opcode) {
		case op_li:   v =  l->eval(d); break;
		case op_not:  v = ~l->eval(d); break;
		case op_neg:  v = -l->eval(d); break;
		default: break;
	}
	return v;
}

Nat binaryop::eval(compiler *d)
{
	Nat v = 0;
	switch(opcode) {
		case op_and:  v = l->eval(d) &  r->eval(d); break;
		case op_or:   v = l->eval(d) |  r->eval(d); break;
		case op_xor:  v = l->eval(d) ^  r->eval(d); break;
		case op_seq:  v = l->eval(d) == r->eval(d); break;
		case op_sne:  v = l->eval(d) != r->eval(d); break;
		case op_slt:  v = l->eval(d) <  r->eval(d); break;
		case op_sle:  v = l->eval(d) <= r->eval(d); break;
		case op_sgt:  v = l->eval(d) >  r->eval(d); break;
		case op_sge:  v = l->eval(d) >= r->eval(d); break;
		case op_srl:  v = l->eval(d) >> r->eval(d).limb_at(0); break;
		case op_srli: v = l->eval(d) >> r->eval(d).limb_at(0); break;
		case op_sll:  v = l->eval(d) << r->eval(d).limb_at(0); break;
		case op_slli: v = l->eval(d) << r->eval(d).limb_at(0); break;
		case op_add:  v = l->eval(d) +  r->eval(d); break;
		case op_sub:  v = l->eval(d) -  r->eval(d); break;
		case op_mul:  v = l->eval(d) *  r->eval(d); break;
		case op_div:  v = l->eval(d) /  r->eval(d); break;
		case op_rem:  v = l->eval(d) %  r->eval(d); break;
		default: break;
	}
	return v;
}

Nat const_int::eval(compiler *d)
{
	return *r;
}

Nat var::eval(compiler *d)
{
	auto vi = d->var_name.find(*l);
	return vi != d->var_name.end() ? vi->second->eval(d) : Nat(0);
}

Nat setvar::eval(compiler *d)
{
	d->var_name[*l] = r.get();
	return r->eval(d);
}

Nat phyreg::eval(compiler *d)
{
	auto ri = d->reg_values.find(l);
	return ri != d->reg_values.end() ? ri->second : Nat(0);
}

Nat setreg::eval(compiler *d)
{
	Nat val = r->eval(d);
	d->reg_values[l->l] = val;
	return val;
}

Nat imm::eval(compiler *d)
{
	return Nat(r);
}


/*-------------------------------------------------------.
| lower nodes into single static assignment form tuples. |
`-------------------------------------------------------*/

node_list node::lower(compiler *)
{
	return node_list();
}

node_list unaryop::lower(compiler *d)
{
	/* get register of operand */
	node_list ll = l->lower(d);
	ssareg *lreg = new ssareg(d->lower_reg(ll));

	/* create new unary op using registers */
	unaryop *op = new unaryop(opcode, lreg);
	setreg *sr = new setreg(new ssareg(d->ssaregcount++), op);

	/* construct node list */
	node_list nodes;
	nodes.insert(nodes.end(), ll.begin(), ll.end());
	nodes.insert(nodes.end(), sr);

	return nodes;
}

node_list binaryop::lower(compiler *d)
{
	/* lower const_int operand as immediate where possible */
	switch (opcode) {
		case op_srl:
		case op_sll:
			if (r->opcode == op_const_int) {
				/* get immediate opcode */
				op imm_opcode;
				switch(opcode) {
					case op_srl: imm_opcode = op_srli; break;
					case op_sll: imm_opcode = op_slli; break;
					default: imm_opcode = op_none; break;
				}

				/* get registers of operand */
				node_list ll = l->lower(d);
				ssareg *lreg = new ssareg(d->lower_reg(ll));
				imm *rimm = new imm(static_cast<const_int*>(r.get())->r->limb_at(0));

				/* create binary op using registers */
				binaryop *op = new binaryop(imm_opcode, lreg, rimm);
				setreg *sr = new setreg(new ssareg(d->ssaregcount++), op);

				/* construct node list */
				node_list nodes;
				nodes.insert(nodes.end(), ll.begin(), ll.end());
				nodes.insert(nodes.end(), sr);

				return nodes;
			}
		default:
			break;
	}

	/* get registers of operands */
	node_list ll = l->lower(d);
	node_list rl = r->lower(d);
	ssareg *lreg = new ssareg(d->lower_reg(ll));
	ssareg *rreg = new ssareg(d->lower_reg(rl));

	/* create binary op using registers */
	binaryop *op = new binaryop(opcode, lreg, rreg);
	setreg *sr = new setreg(new ssareg(d->ssaregcount++), op);

	/* construct node list */
	node_list nodes;
	nodes.insert(nodes.end(), ll.begin(), ll.end());
	nodes.insert(nodes.end(), rl.begin(), rl.end());
	nodes.insert(nodes.end(), sr);

	return nodes;
}

node_list const_int::lower(compiler *d)
{
	/* move the number into a register */
	imm *op_imm = new imm(r->limb_at(0));
	unaryop *op = new unaryop(op_li, op_imm);
	setreg *sr = new setreg(new ssareg(d->ssaregcount++), op);

	return node_list{sr};
}

node_list var::lower(compiler *d)
{
	/* return most recent register for this variable */
	size_t ssaregnum = d->var_ssa[*l];
	return node_list{new ssareg(ssaregnum)};
}

node_list setvar::lower(compiler *d)
{
	/* store most recent register for this variable */
	node_list rl = r->lower(d);
	auto sr = static_cast<setreg*>(rl.back());
	sr->v = std::unique_ptr<var>(new var(*l));
	size_t ssaregnum = sr->l->l;
	d->var_ssa[*l] = ssaregnum;
	return rl;
}


/*-------------------------.
| convert nodes to string. |
`-------------------------*/

std::string unaryop::to_string(compiler *d)
{
	return std::string("(") + op_name[opcode] + " " + l->to_string(d) + ")";
}

std::string binaryop::to_string(compiler *d)
{
	return std::string("(") + op_name[opcode] + " " + l->to_string(d) + ", " + r->to_string(d) + ")";
}

std::string const_int::to_string(compiler *d)
{
	return std::string("(") + op_name[opcode] + " " + r->to_string(16) + ")";
}

std::string var::to_string(compiler *d)
{
	return std::string("(") + op_name[opcode] + " '" + *l + "')";
}

std::string setvar::to_string(compiler *d)
{
	return std::string("(") + op_name[opcode] + " '" + *l + "', " + r->to_string(d) + ")";
}

std::string ssareg::to_string(compiler *d)
{
	return std::string("_") + std::to_string(reg::l);
}

std::string phyreg::to_string(compiler *d)
{
	return d->target->get_reg_name()[reg::l];
}

std::string setreg::to_string(compiler *d)
{
	return std::string("(") + op_name[opcode] + " " + l->to_string(d) + ", " + r->to_string(d) + ")";
}

std::string imm::to_string(compiler *d)
{
	return Nat(r).to_string(16);
}


/*------------------.
| parser interface. |
`------------------*/

compiler::compiler() : ssaregcount(0), phyregcount(31), target(target::backend::get_default()) {}

node* compiler::new_unary(op opcode, node *l)
{
	return new unaryop(opcode, l);
}

node* compiler::new_binary(op opcode, node *l, node *r)
{
	return new binaryop(opcode, l, r);
}

node* compiler::new_const_int(std::string num)
{
	return new const_int(num);
}

node* compiler::set_variable(std::string name, node *r)
{
	setvar *a = new setvar(name, r);
	var_name[name] = a;
	return a;
}

node* compiler::get_variable(std::string name)
{
	auto vi = var_name.find(name);
	if (vi == var_name.end()) error("unknown variable: " + name);
	var *a = new var(name);
	return a;
}

void compiler::add_toplevel(node *n)
{
	nodes.push_back(n);
}

void compiler::error(const location& l, const std::string& m)
{
	std::cerr << l << ": " << m << std::endl;
	exit(1);
}

void compiler::error(const std::string& m)
{
	std::cerr << m << std::endl;
	exit(1);
}


/*-------------------------.
| compiler implementation. |
`-------------------------*/

int compiler::parse(std::istream &in)
{
	lexer scanner;
	scanner.yyrestart(&in);
	parser nat_parser(scanner, *this);
	return nat_parser.parse();
}

void compiler::use_ssa_scan(std::unique_ptr<node> &nr,
	size_t i, size_t j, size_t defreg)
{
	/* scan backwards for register and mark use */
	node *l = nr.get();
	if (l->typecode != type_ssareg) return;
	size_t usereg = static_cast<ssareg*>(l)->l;
	if (usereg != defreg) return;
	def_use_ssa[j * ssaregcount + defreg] = '+';
	for (size_t k = j - 1; k != i; k--) {
		if (def_use_ssa[k * ssaregcount + defreg] == ' ') {
			def_use_ssa[k * ssaregcount + defreg] = '|';
		} else {
			break;
		}
	}
}

void compiler::def_use_ssa_analysis()
{
	/* construct def use matrix */
	size_t def_use_ssa_size = nodes.size() * ssaregcount;
	def_use_ssa = std::unique_ptr<char[]>(new char[def_use_ssa_size]());
	memset(def_use_ssa.get(), ' ', def_use_ssa_size);
	for (size_t i = 0; i < nodes.size(); i++) {
		node *ndef = nodes[i];
		if (ndef->opcode != op_setreg) continue;
		setreg *srdef = static_cast<setreg*>(ndef);
		size_t defreg = srdef->l->l;
		def_use_ssa[i * ssaregcount + defreg] = 'v';
		for (size_t j = i + 1; j < nodes.size(); j++) {
			if (nodes[j]->opcode != op_setreg) continue;
			setreg *sruse = static_cast<setreg*>(nodes[j]);
			node *sruse_op = sruse->r.get();
			if (sruse_op->typecode == type_unaryop) {
				unaryop *use_op = static_cast<unaryop*>(sruse_op);
				use_ssa_scan(use_op->l, i, j, defreg);
			} else if (sruse_op->typecode == type_binaryop) {
				binaryop *use_op = static_cast<binaryop*>(sruse_op);
				use_ssa_scan(use_op->l, i, j, defreg);
				use_ssa_scan(use_op->r, i, j, defreg);
			}
		}
	}
}

void compiler::allocate_registers()
{
	/* create physical registers */
	size_t def_use_phy_size = nodes.size() * phyregcount;
	def_use_phy = std::unique_ptr<char[]>(new char[def_use_phy_size]());
	const size_t *reg = target->get_reg_order();
	while (*reg != 0) {
		reg_values[*reg] = 0;
		reg_free.push_back(*reg++);
	}

	/* loop through nodes to assign physical registers */
	for (size_t i = 0; i < nodes.size(); i++) {
		node *ndef = nodes[i];
		if (ndef->opcode != op_setreg) continue;
		setreg *srdef = static_cast<setreg*>(ndef);

		/* free unused physical registers */
		auto rfi = reg_free.begin();
		for (auto ri = reg_used.begin(); ri != reg_used.end();) {
			size_t ssaregnum = ri->first, phyregnum = ri->second;
			def_use_phy[i * phyregcount + phyregnum] =
				def_use_ssa[i * ssaregcount + ssaregnum];
			if (def_use_ssa[i * ssaregcount + ssaregnum] == ' ') {
				ri = reg_used.erase(ri);
				reg_free.insert(rfi, phyregnum);
			} else {
				ri++;
			}
		}

		/* replace ssa registers with physical registers */
		node *n = srdef->r.get();
		if (n->typecode == type_unaryop) {
			unaryop *op = static_cast<unaryop*>(n);
			node *l = op->l.get();
			if (l->typecode == type_ssareg) {
				size_t ssaregnum = static_cast<ssareg*>(l)->l;
				op->l = std::unique_ptr<phyreg>(new phyreg(reg_used[ssaregnum]));
			}
		} else if (n->typecode == type_binaryop) {
			binaryop *op = static_cast<binaryop*>(n);
			node *l = op->l.get(), *r = op->r.get();
			if (l->typecode == type_ssareg) {
				size_t ssaregnum = static_cast<ssareg*>(l)->l;
				op->l = std::unique_ptr<phyreg>(new phyreg(reg_used[ssaregnum]));
			}
			if (r->typecode == type_ssareg) {
				size_t ssaregnum = static_cast<ssareg*>(r)->l;
				op->r = std::unique_ptr<phyreg>(new phyreg(reg_used[ssaregnum]));
			}
		}

		/* assign new physical register */
		if (reg_free.size() == 0) {
			error("register spilling not implemented");
		}
		size_t ssaregnum = srdef->l->l;
		size_t phyregnum = reg_free.front();
		if (target->get_reg_class()[phyregnum] == target::rs) {
			error("register spilling not implemented");
		}
		reg_free.pop_front();
		def_use_phy[i * phyregcount + phyregnum] =
			def_use_ssa[i * ssaregcount + ssaregnum];
		reg_used[ssaregnum] = phyregnum;
		srdef->l = std::unique_ptr<phyreg>(new phyreg(phyregnum));
	}
}

void compiler::lower(bool regalloc)
{
	/* recursively lower setvar expressions to setreg tuples in SSA form */
	node_list new_nodes;
	for (auto ni = nodes.begin(); ni != nodes.end(); ni++) {
		new_nodes.insert(new_nodes.end(), *ni);
		switch ((*ni)->opcode) {
			case op_setvar: {
				node_list lowered = (*ni)->lower(this);
				new_nodes.insert(new_nodes.end(), lowered.begin(), lowered.end());
				break;
			}
			default:
				break;
		}
	}
	nodes = std::move(new_nodes);

	/* perform def use analysis */
	def_use_ssa_analysis();

	/* allocate physical registers */
	if (regalloc) {
		allocate_registers();
	}
}

size_t compiler::lower_reg(node_list &l)
{
	/* helper function to get register of last node */
	size_t ssaregnum;
	node *n = static_cast<node*>(l.back());
	switch(n->opcode) {
		case op_ssareg:
			ssaregnum = static_cast<ssareg*>(n)->l;
			l.pop_back();
			break;
		case op_setreg:
			ssaregnum = static_cast<setreg*>(n)->l->l;
			break;
		default:
			ssaregnum = 0;
			error("expected reg or setreg node");
			break;
	}
	return ssaregnum;
}

void compiler::run(op opcode)
{
	/* evalulate abstract tree nodes */
	Nat num;
	for (auto n : nodes) {
		if (n->opcode != opcode) continue;
		switch (n->opcode) {
			case op_setvar:
				num = n->eval(this);
				std::cout << " "
					<< *static_cast<setvar*>(n)->l
					<< " = " << num.to_string(10)
					<< " (" << num.to_string(16) << ")" << std::endl;
				break;
			case op_setreg:
				num = n->eval(this);
				std::cout << " "
					<< static_cast<setreg*>(n)->l->to_string(this)
					<< " = " << num.to_string(10)
					<< " (" << num.to_string(16) << ")" << std::endl;
				break;
			default:
				break;
		}
	}
}

void compiler::dump_ast(op opcode, bool regalloc)
{
	/* print abstract syntax tree (op_setvar) or lowered form (op_setreg) */
	for (size_t i = 0; i < nodes.size(); i++) {
		node *n = nodes[i];
		if (n->opcode != opcode) continue;
		switch (n->opcode) {
			case op_setvar: {
				std::cout << "\t" << n->to_string(this) << std::endl;
				break;
			}
			case op_setreg: {
				std::cout << "\t"
					<< std::left << std::setw(40)
					<< n->to_string(this);
				if (regalloc) {
					for (size_t j = 0; j < phyregcount; j++) {
						std::cout << def_use_phy[i * phyregcount + j];
					}
				} else {
					for (size_t j = 0; j < ssaregcount; j++) {
						std::cout << def_use_ssa[i * ssaregcount + j];
					}
				}
				std::cout << std::endl;
				break;
			}
			default:
				break;
		}
	}
}

void compiler::emit_asm()
{
	/* convert lowered form to machine instructions */
	node_list new_nodes;
	for (auto ni = nodes.begin(); ni != nodes.end(); ni++) {
		new_nodes.insert(new_nodes.end(), *ni);
		switch ((*ni)->opcode) {
			case op_setreg: {
				node_list minst = target->emit(this, *ni);
				new_nodes.insert(new_nodes.end(), minst.begin(), minst.end());
				break;
			}
			default:
				break;
		}
	}
	nodes = std::move(new_nodes);
}

void compiler::print_asm()
{
	/* print machine instructions */
	for (size_t i = 0; i < nodes.size(); i++) {
		node *n = nodes[i];
		if (n->opcode != op_mi) continue;
		std::cout << "\t" << n->to_string(this) << std::endl;
	}
}
