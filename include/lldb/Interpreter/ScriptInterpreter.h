//===-- ScriptInterpreter.h -------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_ScriptInterpreter_h_
#define liblldb_ScriptInterpreter_h_

#include "lldb/lldb-private.h"

#include "lldb/Core/Broadcaster.h"
#include "lldb/Core/Error.h"

#include "lldb/Utility/PseudoTerminal.h"


namespace lldb_private {

class ScriptInterpreterObject
{
public:
    ScriptInterpreterObject() :
    m_object(NULL)
    {}
    
    ScriptInterpreterObject(void* obj) :
    m_object(obj)
    {}
    
    ScriptInterpreterObject(const ScriptInterpreterObject& rhs)
    : m_object(rhs.m_object)
    {}
    
    virtual void*
    GetObject()
    {
        return m_object;
    }
    
    explicit operator bool ()
    {
        return m_object != NULL;
    }
    
    ScriptInterpreterObject&
    operator = (const ScriptInterpreterObject& rhs)
    {
        if (this != &rhs)
            m_object = rhs.m_object;
        return *this;
    }
        
    virtual
    ~ScriptInterpreterObject()
    {}
    
protected:
    void* m_object;
};
    
class ScriptInterpreterLocker
{
public:
    
    ScriptInterpreterLocker ()
    {
    }
    
    virtual ~ScriptInterpreterLocker ()
    {
    }
private:
    DISALLOW_COPY_AND_ASSIGN (ScriptInterpreterLocker);
};


class ScriptInterpreter
{
public:

    typedef void (*SWIGInitCallback) (void);

    typedef bool (*SWIGBreakpointCallbackFunction) (const char *python_function_name,
                                                    const char *session_dictionary_name,
                                                    const lldb::StackFrameSP& frame_sp,
                                                    const lldb::BreakpointLocationSP &bp_loc_sp);
    
    typedef bool (*SWIGWatchpointCallbackFunction) (const char *python_function_name,
                                                    const char *session_dictionary_name,
                                                    const lldb::StackFrameSP& frame_sp,
                                                    const lldb::WatchpointSP &wp_sp);
    
    typedef bool (*SWIGPythonTypeScriptCallbackFunction) (const char *python_function_name,
                                                          void *session_dictionary,
                                                          const lldb::ValueObjectSP& valobj_sp,
                                                          void** pyfunct_wrapper,
                                                          const lldb::TypeSummaryOptionsSP& options,
                                                          std::string& retval);
    
    typedef void* (*SWIGPythonCreateSyntheticProvider) (const char *python_class_name,
                                                        const char *session_dictionary_name,
                                                        const lldb::ValueObjectSP& valobj_sp);

    typedef void* (*SWIGPythonCreateScriptedThreadPlan) (const char *python_class_name,
                                                        const char *session_dictionary_name,
                                                        const lldb::ThreadPlanSP& thread_plan_sp);

    typedef bool (*SWIGPythonCallThreadPlan) (void *implementor, const char *method_name, Event *event_sp, bool &got_error);

    typedef void* (*SWIGPythonCreateOSPlugin) (const char *python_class_name,
                                               const char *session_dictionary_name,
                                               const lldb::ProcessSP& process_sp);
    
    typedef uint32_t        (*SWIGPythonCalculateNumChildren)                   (void *implementor);
    typedef void*           (*SWIGPythonGetChildAtIndex)                        (void *implementor, uint32_t idx);
    typedef int             (*SWIGPythonGetIndexOfChildWithName)                (void *implementor, const char* child_name);
    typedef void*           (*SWIGPythonCastPyObjectToSBValue)                  (void* data);
    typedef lldb::ValueObjectSP  (*SWIGPythonGetValueObjectSPFromSBValue)       (void* data);
    typedef bool            (*SWIGPythonUpdateSynthProviderInstance)            (void* data);
    typedef bool            (*SWIGPythonMightHaveChildrenSynthProviderInstance) (void* data);
    typedef void*           (*SWIGPythonGetValueSynthProviderInstance)          (void *implementor);
    
    typedef bool            (*SWIGPythonCallCommand)            (const char *python_function_name,
                                                                 const char *session_dictionary_name,
                                                                 lldb::DebuggerSP& debugger,
                                                                 const char* args,
                                                                 lldb_private::CommandReturnObject& cmd_retobj,
                                                                 lldb::ExecutionContextRefSP exe_ctx_ref_sp);
    
    typedef bool            (*SWIGPythonCallModuleInit)         (const char *python_module_name,
                                                                 const char *session_dictionary_name,
                                                                 lldb::DebuggerSP& debugger);
    
    typedef bool            (*SWIGPythonScriptKeyword_Process)  (const char* python_function_name,
                                                                 const char* session_dictionary_name,
                                                                 lldb::ProcessSP& process,
                                                                 std::string& output);
    typedef bool            (*SWIGPythonScriptKeyword_Thread)   (const char* python_function_name,
                                                                 const char* session_dictionary_name,
                                                                 lldb::ThreadSP& thread,
                                                                 std::string& output);
    
    typedef bool            (*SWIGPythonScriptKeyword_Target)   (const char* python_function_name,
                                                                 const char* session_dictionary_name,
                                                                 lldb::TargetSP& target,
                                                                 std::string& output);

    typedef bool            (*SWIGPythonScriptKeyword_Frame)    (const char* python_function_name,
                                                                 const char* session_dictionary_name,
                                                                 lldb::StackFrameSP& frame,
                                                                 std::string& output);

    typedef bool            (*SWIGPythonScriptKeyword_Value)    (const char* python_function_name,
                                                                 const char* session_dictionary_name,
                                                                 lldb::ValueObjectSP& value,
                                                                 std::string& output);
    
    typedef void*           (*SWIGPython_GetDynamicSetting)     (void* module,
                                                                 const char* setting,
                                                                 const lldb::TargetSP& target_sp);

    typedef enum
    {
        eScriptReturnTypeCharPtr,
        eScriptReturnTypeBool,
        eScriptReturnTypeShortInt,
        eScriptReturnTypeShortIntUnsigned,
        eScriptReturnTypeInt,
        eScriptReturnTypeIntUnsigned,
        eScriptReturnTypeLongInt,
        eScriptReturnTypeLongIntUnsigned,
        eScriptReturnTypeLongLong,
        eScriptReturnTypeLongLongUnsigned,
        eScriptReturnTypeFloat,
        eScriptReturnTypeDouble,
        eScriptReturnTypeChar,
        eScriptReturnTypeCharStrOrNone,
        eScriptReturnTypeOpaqueObject
    } ScriptReturnType;
    
    ScriptInterpreter (CommandInterpreter &interpreter, lldb::ScriptLanguage script_lang);

    virtual ~ScriptInterpreter ();

    struct ExecuteScriptOptions
    {
    public:
        ExecuteScriptOptions () :
            m_enable_io(true),
            m_set_lldb_globals(true),
            m_maskout_errors(true)
        {
        }
        
        bool
        GetEnableIO () const
        {
            return m_enable_io;
        }
        
        bool
        GetSetLLDBGlobals () const
        {
            return m_set_lldb_globals;
        }
        
        bool
        GetMaskoutErrors () const
        {
            return m_maskout_errors;
        }
        
        ExecuteScriptOptions&
        SetEnableIO (bool enable)
        {
            m_enable_io = enable;
            return *this;
        }

        ExecuteScriptOptions&
        SetSetLLDBGlobals (bool set)
        {
            m_set_lldb_globals = set;
            return *this;
        }

        ExecuteScriptOptions&
        SetMaskoutErrors (bool maskout)
        {
            m_maskout_errors = maskout;
            return *this;
        }
        
    private:
        bool m_enable_io;
        bool m_set_lldb_globals;
        bool m_maskout_errors;
    };

    virtual bool
    Interrupt()
    {
        return false;
    }

    virtual bool
    ExecuteOneLine (const char *command,
                    CommandReturnObject *result,
                    const ExecuteScriptOptions &options = ExecuteScriptOptions()) = 0;

    virtual void
    ExecuteInterpreterLoop () = 0;

    virtual bool
    ExecuteOneLineWithReturn (const char *in_string,
                              ScriptReturnType return_type,
                              void *ret_value,
                              const ExecuteScriptOptions &options = ExecuteScriptOptions())
    {
        return true;
    }

    virtual Error
    ExecuteMultipleLines (const char *in_string,
                          const ExecuteScriptOptions &options = ExecuteScriptOptions())
    {
        Error error;
        error.SetErrorString("not implemented");
        return error;
    }

    virtual Error
    ExportFunctionDefinitionToInterpreter (StringList &function_def)
    {
        Error error;
        error.SetErrorString("not implemented");
        return error;
    }

    virtual Error
    GenerateBreakpointCommandCallbackData (StringList &input, std::string& output)
    {
        Error error;
        error.SetErrorString("not implemented");
        return error;
    }
    
    virtual bool
    GenerateWatchpointCommandCallbackData (StringList &input, std::string& output)
    {
        return false;
    }
    
    virtual bool
    GenerateTypeScriptFunction (const char* oneliner, std::string& output, void* name_token = NULL)
    {
        return false;
    }
    
    virtual bool
    GenerateTypeScriptFunction (StringList &input, std::string& output, void* name_token = NULL)
    {
        return false;
    }
    
    virtual bool
    GenerateScriptAliasFunction (StringList &input, std::string& output)
    {
        return false;
    }
    
    virtual bool
    GenerateTypeSynthClass (StringList &input, std::string& output, void* name_token = NULL)
    {
        return false;
    }
    
    virtual bool
    GenerateTypeSynthClass (const char* oneliner, std::string& output, void* name_token = NULL)
    {
        return false;
    }
    
    virtual lldb::ScriptInterpreterObjectSP
    CreateSyntheticScriptedProvider (const char *class_name,
                                     lldb::ValueObjectSP valobj)
    {
        return lldb::ScriptInterpreterObjectSP();
    }
    
    virtual lldb::ScriptInterpreterObjectSP
    OSPlugin_CreatePluginObject (const char *class_name,
                                 lldb::ProcessSP process_sp)
    {
        return lldb::ScriptInterpreterObjectSP();
    }
    
    virtual lldb::ScriptInterpreterObjectSP
    OSPlugin_RegisterInfo (lldb::ScriptInterpreterObjectSP os_plugin_object_sp)
    {
        return lldb::ScriptInterpreterObjectSP();
    }
    
    virtual lldb::ScriptInterpreterObjectSP
    OSPlugin_ThreadsInfo (lldb::ScriptInterpreterObjectSP os_plugin_object_sp)
    {
        return lldb::ScriptInterpreterObjectSP();
    }
    
    virtual lldb::ScriptInterpreterObjectSP
    OSPlugin_RegisterContextData (lldb::ScriptInterpreterObjectSP os_plugin_object_sp,
                                  lldb::tid_t thread_id)
    {
        return lldb::ScriptInterpreterObjectSP();
    }

    virtual lldb::ScriptInterpreterObjectSP
    OSPlugin_CreateThread (lldb::ScriptInterpreterObjectSP os_plugin_object_sp,
                           lldb::tid_t tid,
                           lldb::addr_t context)
    {
        return lldb::ScriptInterpreterObjectSP();
    }
    
    virtual lldb::ScriptInterpreterObjectSP
    CreateScriptedThreadPlan (const char *class_name,
                              lldb::ThreadPlanSP thread_plan_sp)
    {
        return lldb::ScriptInterpreterObjectSP();
    }

    virtual bool
    ScriptedThreadPlanExplainsStop (lldb::ScriptInterpreterObjectSP implementor_sp,
                                    Event *event,
                                    bool &script_error)
    {
        script_error = true;
        return true;
    }

    virtual bool
    ScriptedThreadPlanShouldStop (lldb::ScriptInterpreterObjectSP implementor_sp,
                                  Event *event,
                                  bool &script_error)
    {
        script_error = true;
        return true;
    }

    virtual lldb::StateType
    ScriptedThreadPlanGetRunState (lldb::ScriptInterpreterObjectSP implementor_sp,
                                   bool &script_error)
    {
        script_error = true;
        return lldb::eStateStepping;
    }

    virtual lldb::ScriptInterpreterObjectSP
    LoadPluginModule (const FileSpec& file_spec,
                     lldb_private::Error& error)
    {
        return lldb::ScriptInterpreterObjectSP();
    }
    
    virtual lldb::ScriptInterpreterObjectSP
    GetDynamicSettings (lldb::ScriptInterpreterObjectSP plugin_module_sp,
                        Target* target,
                        const char* setting_name,
                        lldb_private::Error& error)
    {
        return lldb::ScriptInterpreterObjectSP();
    }

    virtual Error
    GenerateFunction(const char *signature, const StringList &input)
    {
        Error error;
        error.SetErrorString("unimplemented");
        return error;
    }

    virtual void 
    CollectDataForBreakpointCommandCallback (std::vector<BreakpointOptions *> &options,
                                             CommandReturnObject &result);

    virtual void 
    CollectDataForWatchpointCommandCallback (WatchpointOptions *wp_options,
                                             CommandReturnObject &result);

    /// Set the specified text as the callback for the breakpoint.
    Error
    SetBreakpointCommandCallback (std::vector<BreakpointOptions *> &bp_options_vec,
                                  const char *callback_text);

    virtual Error
    SetBreakpointCommandCallback (BreakpointOptions *bp_options,
                                  const char *callback_text)
    {
        Error error;
        error.SetErrorString("unimplemented");
        return error;
    }
    
    void
    SetBreakpointCommandCallbackFunction (std::vector<BreakpointOptions *> &bp_options_vec,
                                  const char *function_name);

    /// Set a one-liner as the callback for the breakpoint.
    virtual void 
    SetBreakpointCommandCallbackFunction (BreakpointOptions *bp_options,
                                  const char *function_name)
    {
        return;
    }
    
    /// Set a one-liner as the callback for the watchpoint.
    virtual void 
    SetWatchpointCommandCallback (WatchpointOptions *wp_options,
                                  const char *oneliner)
    {
        return;
    }
    
    virtual bool
    GetScriptedSummary (const char *function_name,
                        lldb::ValueObjectSP valobj,
                        lldb::ScriptInterpreterObjectSP& callee_wrapper_sp,
                        const TypeSummaryOptions& options,
                        std::string& retval)
    {
        return false;
    }
    
    virtual void
    Clear ()
    {
        // Clean up any ref counts to SBObjects that might be in global variables
    }
    
    virtual size_t
    CalculateNumChildren (const lldb::ScriptInterpreterObjectSP& implementor)
    {
        return 0;
    }
    
    virtual lldb::ValueObjectSP
    GetChildAtIndex (const lldb::ScriptInterpreterObjectSP& implementor, uint32_t idx)
    {
        return lldb::ValueObjectSP();
    }
    
    virtual int
    GetIndexOfChildWithName (const lldb::ScriptInterpreterObjectSP& implementor, const char* child_name)
    {
        return UINT32_MAX;
    }
    
    virtual bool
    UpdateSynthProviderInstance (const lldb::ScriptInterpreterObjectSP& implementor)
    {
        return false;
    }
    
    virtual bool
    MightHaveChildrenSynthProviderInstance (const lldb::ScriptInterpreterObjectSP& implementor)
    {
        return true;
    }
    
    virtual lldb::ValueObjectSP
    GetSyntheticValue (const lldb::ScriptInterpreterObjectSP& implementor)
    {
        return nullptr;
    }
    
    virtual bool
    RunScriptBasedCommand (const char* impl_function,
                           const char* args,
                           ScriptedCommandSynchronicity synchronicity,
                           lldb_private::CommandReturnObject& cmd_retobj,
                           Error& error,
                           const lldb_private::ExecutionContext& exe_ctx)
    {
        return false;
    }
    
    virtual bool
    RunScriptFormatKeyword (const char* impl_function,
                            Process* process,
                            std::string& output,
                            Error& error)
    {
        error.SetErrorString("unimplemented");
        return false;
    }

    virtual bool
    RunScriptFormatKeyword (const char* impl_function,
                            Thread* thread,
                            std::string& output,
                            Error& error)
    {
        error.SetErrorString("unimplemented");
        return false;
    }
    
    virtual bool
    RunScriptFormatKeyword (const char* impl_function,
                            Target* target,
                            std::string& output,
                            Error& error)
    {
        error.SetErrorString("unimplemented");
        return false;
    }
    
    virtual bool
    RunScriptFormatKeyword (const char* impl_function,
                            StackFrame* frame,
                            std::string& output,
                            Error& error)
    {
        error.SetErrorString("unimplemented");
        return false;
    }
    
    virtual bool
    RunScriptFormatKeyword (const char* impl_function,
                            ValueObject* value,
                            std::string& output,
                            Error& error)
    {
        error.SetErrorString("unimplemented");
        return false;
    }
    
    virtual bool
    GetDocumentationForItem (const char* item, std::string& dest)
    {
		dest.clear();
        return false;
    }
    
    virtual bool
    CheckObjectExists (const char* name)
    {
        return false;
    }

    virtual bool
    LoadScriptingModule (const char* filename,
                         bool can_reload,
                         bool init_session,
                         lldb_private::Error& error,
                         lldb::ScriptInterpreterObjectSP* module_sp = nullptr)
    {
        error.SetErrorString("loading unimplemented");
        return false;
    }

    virtual lldb::ScriptInterpreterObjectSP
    MakeScriptObject (void* object)
    {
        return lldb::ScriptInterpreterObjectSP(new ScriptInterpreterObject(object));
    }
    
    virtual std::unique_ptr<ScriptInterpreterLocker>
    AcquireInterpreterLock ();
    
    const char *
    GetScriptInterpreterPtyName ();

    int
    GetMasterFileDescriptor ();

	CommandInterpreter &
	GetCommandInterpreter ();

    static std::string
    LanguageToString (lldb::ScriptLanguage language);
    
    static void
    InitializeInterpreter (SWIGInitCallback python_swig_init_callback,
                           SWIGBreakpointCallbackFunction swig_breakpoint_callback,
                           SWIGWatchpointCallbackFunction swig_watchpoint_callback,
                           SWIGPythonTypeScriptCallbackFunction swig_typescript_callback,
                           SWIGPythonCreateSyntheticProvider swig_synthetic_script,
                           SWIGPythonCalculateNumChildren swig_calc_children,
                           SWIGPythonGetChildAtIndex swig_get_child_index,
                           SWIGPythonGetIndexOfChildWithName swig_get_index_child,
                           SWIGPythonCastPyObjectToSBValue swig_cast_to_sbvalue ,
                           SWIGPythonGetValueObjectSPFromSBValue swig_get_valobj_sp_from_sbvalue,
                           SWIGPythonUpdateSynthProviderInstance swig_update_provider,
                           SWIGPythonMightHaveChildrenSynthProviderInstance swig_mighthavechildren_provider,
                           SWIGPythonGetValueSynthProviderInstance swig_getvalue_provider,
                           SWIGPythonCallCommand swig_call_command,
                           SWIGPythonCallModuleInit swig_call_module_init,
                           SWIGPythonCreateOSPlugin swig_create_os_plugin,
                           SWIGPythonScriptKeyword_Process swig_run_script_keyword_process,
                           SWIGPythonScriptKeyword_Thread swig_run_script_keyword_thread,
                           SWIGPythonScriptKeyword_Target swig_run_script_keyword_target,
                           SWIGPythonScriptKeyword_Frame swig_run_script_keyword_frame,
                           SWIGPythonScriptKeyword_Value swig_run_script_keyword_value,
                           SWIGPython_GetDynamicSetting swig_plugin_get,
                           SWIGPythonCreateScriptedThreadPlan swig_thread_plan_script,
                           SWIGPythonCallThreadPlan swig_call_thread_plan);

    virtual void
    ResetOutputFileHandle (FILE *new_fh) { } //By default, do nothing.

protected:
    CommandInterpreter &m_interpreter;
    lldb::ScriptLanguage m_script_lang;
};

} // namespace lldb_private

#endif // #ifndef liblldb_ScriptInterpreter_h_
