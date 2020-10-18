#include "cpp.h"

namespace Cpp
{
static inline std::string toHexString(int x)
{
	std::stringstream ss;
	ss << std::hex << std::showbase << x;
	return ss.str();
}

std::string File::toString(bool justUserTypes, bool includeComments)
{
	std::stringstream ss;

	// Write class/enum declarations
	for (UserType *ut : userTypes)
	{
		if (ut->type == UserType::CLASS ||
			ut->type == UserType::UNION ||
			ut->type == UserType::STRUCT || 
			ut->type == UserType::ENUM)
			ss << ut->toDeclarationString() << "\n";
	}

	ss << "\n";

	// Write function type definitions
	for (UserType *ut : userTypes)
	{
		if (ut->type == UserType::FUNCTION)
			ss << ut->toDeclarationString() << "\n";
	}

	ss << "\n";

	// Write array type declarations
	for (UserType *ut : userTypes)
	{
		if (ut->type == UserType::ARRAY)
			ss << ut->toDeclarationString() << "\n";
	}

	ss << "\n";

	// Write class/enum definitions
	for (UserType *ut : userTypes)
	{
		if (ut->type == UserType::CLASS ||
			ut->type == UserType::UNION ||
			ut->type == UserType::STRUCT ||
			ut->type == UserType::ENUM)
		{
			ss << ut->toDefinitionString(includeComments) << "\n\n";
		}
	}

	if (!justUserTypes)
	{
		// Write variables
		for (Variable &var : variables)
		{
			if (includeComments)
			{
				std::string globallocal = (var.isGlobal) ? "GLOBAL" : "LOCAL ";
				ss << StarCommentToString(globallocal, false) << " ";
			}
			
			ss << var.toString() << ";\n";
		}

		ss << "\n";

		// Write function declarations
		for (Function &fun : functions)
		{
			if (includeComments)
			{
				std::string globallocal = (fun.isGlobal) ? "GLOBAL" : "LOCAL ";
				ss << StarCommentToString(globallocal, false) << " ";
			}
			ss << fun.toDeclarationString() << "\n";
		}

		ss << "\n";

		// Write function definitions
		for (Function &fun : functions)
			ss << fun.toDefinitionString() << "\n\n";
	}

	return ss.str();
}

std::string Type::toString()
{
	std::string name = (isFundamentalType ?
		FundamentalTypeToString(fundamentalType) :
		userType->name);

	std::vector<std::string> ptrref;
	std::vector<std::string> prefix;
	std::vector<std::string> postfix;

	bool isFuncPointer = (isFundamentalType) ? false :
		(userType->type == UserType::FUNCTION);

	bool firstPointerFound = false;

	for (Modifier mod : modifiers)
	{
		std::string modstr = ModifierToString(mod);

		if (mod == CONST && mod == VOLATILE)
		{
			if (prefix.size() == 0)
				prefix.push_back(modstr);
			else
				postfix.push_back(modstr);
		}
		else
		{
			// Kind of a hack to make function pointers print correctly.
			// In the real world, a function pointer is typically defined as a pointer
			// in the typedef itself, and not in a variable/member type...
			// So this ignores the first pointer from the modifiers.
			// See FunctionType::toNameString()... it prints the function type as a pointer
			// even though it really isn't.

			if (isFuncPointer && !firstPointerFound)
				firstPointerFound = true;
			else
				ptrref.push_back(modstr);
		}
	}

	std::stringstream ss;

	for (std::string s : prefix)
		ss << s << " ";

	ss << name;

	for (std::string s : ptrref)
		ss << s;

	ss << " ";

	for (std::string s : postfix)
		ss << s << " ";

	std::string str = ss.str();
	str.pop_back();

	return str;
}

std::string Variable::toString()
{
	std::stringstream ss;
	ss << type.toString() << " " << name;
	return ss.str();
}

std::string UserType::toDeclarationString()
{
	std::stringstream ss;
	ss << "typedef " << toNameString(false, false) << ";";
	return ss.str();
}

std::string UserType::toDefinitionString(bool includeComments)
{
	std::stringstream ss;
	ss << toNameString(includeComments, true) << "\n";

	switch (type)
	{
	case UNION:
	case STRUCT:
	case CLASS:
		ss << classData->toBodyString(includeComments);
		break;
	case ENUM:
		ss << enumData->toBodyString();
		break;
	}

	ss << ";";

	return ss.str();
}

std::string UserType::toNameString(bool includeSize, bool includeInheritances)
{
	switch (type)
	{
	case UNION:
	case STRUCT:
	case CLASS:
		return classData->toNameString(name, includeSize, includeInheritances);
	case ENUM:
		return enumData->toNameString(name);
	case ARRAY:
		return arrayData->toNameString(name);
	case FUNCTION:
		return functionData->toNameString(name);
	}

	return "<unknown user type (" + toHexString(type) + ")>";
}

std::string ClassType::toNameString(std::string name, bool includeSize, bool includeInheritances)
{
	std::stringstream ss;
	ss << std::string((parent->type == UserType::STRUCT) ? "struct " : ((parent->type == UserType::UNION) ? "union " : "class ")) << name;

	if (includeInheritances)
	{
		for (size_t i = 0; i < inheritances.size(); i++)
		{
			std::string type = inheritances[i].type.toString();

			if (i == 0)
				ss << " : " << type;
			else
				ss << ", " << type;
		}
	}
	
	if (includeSize)
		ss << " " + StarCommentToString(toHexString(size), false);

	return ss.str();
}

std::string ClassType::toBodyString(bool includeOffsets)
{
	std::stringstream ss;
	ss << "{\n";

	bool includeUnions = (parent->type != UserType::UNION);
	int unionOffset = -1;

	size_t size = members.size();

	for (size_t i = 0; i < size; i++)
	{
		ss << "\t";

		Member &m = members[i];
		int offset = m.offset;

		if (includeUnions && offset != unionOffset &&
			i < size - 1 && members[i + 1].offset == offset)
		{
			unionOffset = offset;

			if (m.bit_size == -1) {
				ss << "union";
			}
			else {
				ss << "struct";
			}

			ss << "\n\t{\n\t";
		}

		if (includeUnions && unionOffset != -1)
			ss << "\t";

		ss << m.toString(includeOffsets) << ";\n";

		if (includeUnions && unionOffset != -1 &&
			(i == size - 1 || members[i+1].offset != offset))
		{
			unionOffset = -1;
			ss << "\t};\n";
		}
	}

	if (functions.size() > 0) {
		ss << "\n";
		for (Function& fun : functions) {
			ss << "\t" << fun.toDeclarationString() << ";\n";
		}
	}

	ss << "}";

	return ss.str();
}

std::string ClassType::Member::toString(bool includeOffset)
{
	std::stringstream ss;

	if (includeOffset)
		ss << StarCommentToString(toHexString(offset), false) << " ";

	ss << type.toString() << " " << name;
	if (bit_size != -1)
		ss << " : " << bit_size;

	return ss.str();
}

std::string EnumType::toNameString(std::string name)
{
	std::stringstream ss;
	ss << "enum " << name;
	if (baseType != Cpp::FundamentalType::INT)
		ss << " : " << FundamentalTypeToString(baseType);
	return ss.str();
}

std::string EnumType::toBodyString()
{
	std::stringstream ss;

	ss << "{\n";

	int lastValue = -1;
	size_t size = elements.size();

	for (size_t i = 0; i < size; i++)
	{
		ss << "\t" << elements[i].toString(lastValue);

		lastValue = elements[i].constValue;

		if (i != size - 1)
			ss << ",";

		ss << "\n";
	}

	ss << "}";

	return ss.str();
}

std::string EnumType::Element::toString(int lastValue)
{
	std::stringstream ss;
	ss << name;

	if (constValue != lastValue + 1)
		ss << " = " << toHexString(constValue);

	return ss.str();
}

std::string ArrayType::toNameString(std::string name)
{
	std::stringstream ss;
	ss << type.toString() << " " << name;

	for (Dimension &d : dimensions)
		ss << "[" << std::to_string(d.size) << "]";

	return ss.str();
}

std::string FunctionType::toNameString(std::string name)
{
	std::stringstream ss;

	// This isn't really a function pointer, but we'll print it as if it is
	// DWARF is weird
	ss << returnType.toString() << "(*" << name << ")" << toParametersString();
	return ss.str();
}

std::string FunctionType::toParametersString()
{
	std::stringstream ss;
	ss << "(";

	size_t size = parameters.size();

	for (size_t i = 0; i < size; i++)
	{
		ss << parameters[i].toString();

		if (i != size - 1)
			ss << ", ";
	}

	ss << ")";

	return ss.str();
}

std::string FunctionType::Parameter::toString()
{
	std::stringstream ss;
	ss << type.toString();

	if (!name.empty())
		ss << " " << name;

	return ss.str();
}

std::string Function::toNameString(bool skipNamespace)
{
	std::stringstream ss;
	ss << returnType.toString() << " ";
	if (typeOwner != nullptr && !skipNamespace)
		ss << typeOwner->name << "::";
	ss << name << toParametersString();
	return ss.str();
}

std::string Function::toNameString()
{
	return toNameString(false);
}

std::string Function::toDeclarationString()
{
	std::stringstream ss;
	ss << toNameString(true) << ";";
	return ss.str();
}

std::string Function::toDefinitionString()
{
	std::stringstream ss;
	ss << CommentToString(mangledName) <<
		CommentToString("Start address: " + toHexString(startAddress)) <<
		toNameString() << "\n{\n";

	for (Variable &v : variables)
		ss << "\t" << v.toString() << ";\n";

	ss << "}";

	return ss.str();
}

std::string FundamentalTypeToString(FundamentalType ft)
{
	switch (ft)
	{
	case FundamentalType::CHAR:
	case FundamentalType::SIGNED_CHAR:
		return "char";
	case FundamentalType::UNSIGNED_CHAR:
		return "unsigned char";
	case FundamentalType::SHORT:
	case FundamentalType::SIGNED_SHORT:
		return "short";
	case FundamentalType::UNSIGNED_SHORT:
		return "unsigned short";
	case FundamentalType::INT:
	case FundamentalType::SIGNED_INT:
		return "int";
	case FundamentalType::UNSIGNED_INT:
		return "unsigned int";
	case FundamentalType::LONG:
	case FundamentalType::SIGNED_LONG:
		return "long";
	case FundamentalType::UNSIGNED_LONG:
		return "unsigned long";
	case FundamentalType::FLOAT:
		return "float";
	case FundamentalType::DOUBLE:
		return "double";
	case FundamentalType::LONG_DOUBLE:
		return "long double";
	case FundamentalType::VOID:
		return "void";
	case FundamentalType::BOOL:
		return "bool";
	case FundamentalType::LONG_LONG:
	case FundamentalType::SIGNED_LONG_LONG:
		return "long long";
	case FundamentalType::UNSIGNED_LONG_LONG:
		return "unsigned long long";
	}

	std::stringstream ss;
	ss << "<unknown type (" << toHexString(ft) << ")>";
	return ss.str();
}

int GetFundamentalTypeSize(FundamentalType ft)
{
	switch (ft)
	{
	case FundamentalType::CHAR:
	case FundamentalType::SIGNED_CHAR:
	case FundamentalType::UNSIGNED_CHAR:
		return 1;
	case FundamentalType::SHORT:
	case FundamentalType::SIGNED_SHORT:
	case FundamentalType::UNSIGNED_SHORT:
		return 2;
	case FundamentalType::INT:
	case FundamentalType::SIGNED_INT:
	case FundamentalType::UNSIGNED_INT:
		return 4;
	case FundamentalType::LONG:
	case FundamentalType::SIGNED_LONG:
	case FundamentalType::UNSIGNED_LONG:
		return 8; // Confirmed 8 bytes.
	case FundamentalType::FLOAT:
		return 4;
	case FundamentalType::DOUBLE:
		return 8;
	case FundamentalType::LONG_DOUBLE:
		return 8; // TODO: UNSURE
	case FundamentalType::VOID:
		return 4; // TODO: UNSURE
	case FundamentalType::BOOL:
		return 1; // TODO: UNSURE
	case FundamentalType::LONG_LONG:
	case FundamentalType::SIGNED_LONG_LONG:
	case FundamentalType::UNSIGNED_LONG_LONG:
		return 8; // TODO: UNSURE
	}

	return -1;
}

int Type::size() {
	if (modifiers.size() > 0)
		for (Cpp::Type::Modifier modifier : modifiers)
			if (modifier == Cpp::Type::Modifier::POINTER_TO || modifier == Cpp::Type::Modifier::REFERENCE_TO)
				return 4;

	if (isFundamentalType) {
		return GetFundamentalTypeSize(fundamentalType);
	}
	else {
		switch (userType->type) {
		case Cpp::UserType::STRUCT:
		case Cpp::UserType::CLASS:
		case Cpp::UserType::UNION:
			return userType->classData->size;
			break;
		case Cpp::UserType::ARRAY:
			int amount;
			amount = 1;
			if (userType->arrayData->dimensions.size() > 0)
				for (auto dimension : userType->arrayData->dimensions)
					amount *= dimension.size;
			return amount * userType->arrayData->type.size();
			break;
		case Cpp::UserType::FUNCTION:
			return 4;
			break;
		case Cpp::UserType::ENUM:
			return GetFundamentalTypeSize(userType->enumData->baseType);
			break;
		}
	}
}

std::string Type::ModifierToString(Modifier m)
{
	switch (m)
	{
	case CONST:
		return "const";
	case POINTER_TO:
		return "*";
	case REFERENCE_TO:
		return "&";
	case VOLATILE:
		return "volatile";
	}

	std::stringstream ss;
	ss << "<unknown modifier (" << toHexString(m) << ")>";
	return ss.str();
}

std::string CommentToString(std::string comment)
{
	return "// " + comment + "\n";
}

std::string StarCommentToString(std::string comment, bool multiline)
{
	if (multiline)
		return "/*\n" + comment + "\n*/\n";
	else
		return "/* " + comment + " */";
}
}