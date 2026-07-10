#ifndef ATG_ENGINE_SIM_OBJECT_REFERENCE_H
#define ATG_ENGINE_SIM_OBJECT_REFERENCE_H

#include "node.h"

#include "object_reference_node_output.h"

namespace es_script {

    template <typename Type>
    class ObjectReferenceNode : public Node {
    public:
        ObjectReferenceNode() {
            /* void */
        }

        ~ObjectReferenceNode() {
            /* void */
        }

        // PORT NOTE (M0 macOS bring-up): renamed the template parameter from
        // `Type` to `T_OverrideType` -- it shadowed the enclosing class
        // template's own `Type` parameter, which Clang rejects outright
        // ("declaration of 'Type' shadows template parameter") rather than
        // merely warning about it as MSVC apparently did. Purely a rename of
        // the parameter's internal name; callers use explicit template
        // arguments (overrideType<Foo>()) unaffected by this.
        template <typename T_OverrideType>
        void overrideType() {
            m_output.overrideType(LookupChannelType<T_OverrideType>());
        }

    protected:
        virtual void registerOutputs() {
            setPrimaryOutput("__out");
            registerOutput(&m_output, "__out");
        }

        virtual void _evaluate() {
            m_output.setReference(nullptr);
        }

        void setOutput(Type *output) { m_output.setReference(output); }

        ObjectReferenceNodeOutput<Type> m_output;
    };

    template <typename T_ReferenceType>
    using NullReferenceNode = ObjectReferenceNode<T_ReferenceType>;

} /* namespace manta */

#endif /* ATG_ENGINE_SIM_OBJECT_REFERENCE_H */
