#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>

using namespace std;

namespace runtime {

ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
    : data_(std::move(data)) {
}

void ObjectHolder::AssertIsValid() const {
    assert(data_ != nullptr);
}

ObjectHolder ObjectHolder::Share(Object& object) {
    // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
    return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
}

ObjectHolder ObjectHolder::None() {
    return ObjectHolder();
}

Object& ObjectHolder::operator*() const {
    AssertIsValid();
    return *Get();
}

Object* ObjectHolder::operator->() const {
    AssertIsValid();
    return Get();
}

Object* ObjectHolder::Get() const {
    return data_.get();
}

ObjectHolder::operator bool() const {
    return Get() != nullptr;
}

bool IsTrue(const ObjectHolder& value) {
    if (bool(value)){
        if (const Number* num = value.TryAs<Number>()){
            return num->GetValue();
        }
        if (const String* str = value.TryAs<String>()){
            return !str->GetValue().empty();
        }
        if (const Bool* b = value.TryAs<Bool>()){
            return b->GetValue();
        }
    }
    return false;
}

void ClassInstance::Print(std::ostream& os, Context& context) {
    if (HasMethod("__str__"s, 0)) {
           const Method* m = inst_class_.GetMethod("__str__"s);
           Closure fields;
       fields["self"s] = ObjectHolder::Share(*this);
           m->body.get()->Execute(fields, context).Get()->Print(os, context);
       }
       else {
           os << &(*this);
       }

}

bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
    const Method* met = inst_class_.GetMethod(method);
        if (met) {
            return (met->formal_params.size() == argument_count);
        }
        return false;
}

Closure& ClassInstance::Fields() {
    return inst_fields_;
}

const Closure& ClassInstance::Fields() const {
   return inst_fields_;
}

ClassInstance::ClassInstance(const Class& cls) : inst_class_(cls){}

ObjectHolder ClassInstance::Call(const std::string& method,
                                 const std::vector<ObjectHolder>& actual_args,
                                 Context& context) {
    if (HasMethod(method, actual_args.size())) {
            const Method* m = inst_class_.GetMethod(method);
            const auto& params = m->formal_params;
            Closure closure;
            closure["self"s] = ObjectHolder::Share(*this);
            for (size_t i = 0; i < params.size(); ++i) {
                closure[params[i]] = actual_args[i];
            }
            return m->body.get()->Execute(closure, context);
        }
        else
            throw std::runtime_error("Cannot call "s + method);

 }

Class::Class(std::string name, std::vector<Method> methods, const Class* parent) :
    name_cl_(std::move(name)),cl_methods_(std::move(methods)),cl_parent_(parent)  {}

bool operator==(const Method& method, const std::string& name) {
    return method.name == name;
}


const Method* Class::GetMethod(const std::string& name) const {
    auto it = std::find(cl_methods_.begin(), cl_methods_.end(), name);
        if (it != cl_methods_.end()) {
            return &(*it);
        }
        else if (cl_parent_) {
            return cl_parent_->GetMethod(name);
        }
        return nullptr;

}

[[nodiscard]] const std::string& Class::GetName() const {
   return name_cl_;
}

void Class::Print(ostream& os, [[maybe_unused]] Context& context) {
    os << "Class "s << GetName();

}

void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
    os << (GetValue() ? "True"sv : "False"sv);
}

bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    if (!bool(lhs)) {
        if (!bool(rhs)) {
            return true;
        }
    }
    else if (!bool(rhs)) {
        throw std::runtime_error("Cannot compare objects for equality with None"s);
    }
    else if (ClassInstance* v = lhs.TryAs<ClassInstance>()) {
        if (v->HasMethod("__eq__"s, 1)) {
            ObjectHolder result = v->Call("__eq__"s, { rhs }, context);
            return result.TryAs<ValueObject<bool>>()->GetValue();
        }
    }
    else if (const Class* v = lhs.TryAs<Class>()) {
        if (const Method* m = v->GetMethod("__eq__"s)) {
            Closure fields;
            fields["__eq__"s] = rhs;
            ObjectHolder result = m->body.get()->Execute(fields, context);
            return result.TryAs<ValueObject<bool>>()->GetValue();
        }
    }
    else if (const String* lhs_num = lhs.TryAs<String>()) {
        if (const String* rhs_num = rhs.TryAs<String>()) {
            return *lhs_num == *rhs_num;
        }
    }
    else if (const Number* lhs_num = lhs.TryAs<Number>()) {
        if (const Number* rhs_num = rhs.TryAs<Number>()) {
            return *lhs_num == *rhs_num;
        }
    }
    else if (const Bool* lhs_num = lhs.TryAs<Bool>()) {
        if (const Bool* rhs_num = rhs.TryAs<Bool>()) {
            return *lhs_num == *rhs_num;
        }
    }
    throw std::runtime_error("Cannot compare objects for equality in general"s);
}

bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    if (!bool(lhs) || !bool(rhs)) {
        throw std::runtime_error("Cannot compare objects for less"s);
    }
    else if (ClassInstance* v = lhs.TryAs<ClassInstance>()) {
        if (v->HasMethod("__lt__"s, 1)) {
            ObjectHolder result = v->Call("__lt__"s, { rhs }, context);
            return result.TryAs<ValueObject<bool>>()->GetValue();
        }
    }
    else if (const Class* v = lhs.TryAs<Class>()) {
        if (const Method* m = v->GetMethod("__lt__"s)) {
            Closure fields;
            fields["__lt__"s] = rhs;
            ObjectHolder result = m->body.get()->Execute(fields, context);
            return result.TryAs<ValueObject<bool>>()->GetValue();
        }
    }
    else if (const String* lhs_val = lhs.TryAs<String>()) {
        if (const String* rhs_val = rhs.TryAs<String>()) {
            return *lhs_val < *rhs_val;
        }
    }
    else if (const Number* lhs_val = lhs.TryAs<Number>()) {
        if (const Number* rhs_val = rhs.TryAs<Number>()) {
            return *lhs_val < *rhs_val;
        }
    }
    else if (const Bool* lhs_val = lhs.TryAs<Bool>()) {
        if (const Bool* rhs_val = rhs.TryAs<Bool>()) {
            return *lhs_val < *rhs_val;
        }
    }
    throw std::runtime_error("Cannot compare objects for less"s);
}

bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    try {
        return !Equal(lhs, rhs, context);
    }
    catch (...) {
        throw std::runtime_error("Cannot compare objects for NotEqual"s);
    }
}


bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    try {
        return  !Less(lhs, rhs, context) && !Equal(lhs, rhs, context);
    }
    catch (...) {
        throw std::runtime_error("Cannot compare objects for Greater"s);
    }
}

bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    try {
        return !Greater(lhs, rhs, context);
    }
    catch (...) {
        throw std::runtime_error("Cannot compare objects for LessOrEqual"s);
    }
}

bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    try {
        return !Less(lhs, rhs, context);
    }
    catch (...) {
        throw std::runtime_error("Cannot compare objects for GreaterOrEqual"s);
    }
}


}  // namespace runtime
