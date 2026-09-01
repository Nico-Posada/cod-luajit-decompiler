class Lua {
  public:
    Lua(const Bytecode& bytecode,
        const Ast& ast,
        const std::string& filePath,
        const bool& forceOverwrite,
        const bool& minimizeDiffs);

    void operator()();

    const std::string filePath;

  private:
#ifdef _WIN32
    static constexpr char NEW_LINE[] = "\r\n";
#else
    static constexpr char NEW_LINE[] = "\n";
#endif

    void write_header();
    void write_block(const Ast::Function& function, const std::vector<Ast::Statement*>& block);
    void write_expression(const Ast::Expression& expression, const bool& useParentheses);
    void write_prefix_expression(const Ast::Expression& expression, const bool& isLineStart);
    void write_variable(const Ast::Variable& variable, const bool& isLineStart);
    void write_function_call(const Ast::FunctionCall& functionCall, const bool& isLineStart);
    void write_assignment(
        const std::vector<Ast::Variable>& variables,
        const std::vector<Ast::Expression*>& expressions,
        const std::string& separator,
        const bool& isLineStart
    );
    void write_expression_list(const std::vector<Ast::Expression*>& expressions, const Ast::Expression* const& multres);
    void write_function_definition(const Ast::Function& function, const bool& isMethod);
    void write_number(const double& number);
    void write_string(const std::string& string);
    void write_name(const Ast::Constant& constant);
    uint8_t get_operator_precedence(const Ast::Expression& expression);
    void write(const std::string& string);
    template <typename... Strings> void write(const std::string& string, const Strings&... strings);
    void write_indent();
    void write_file();

    const Bytecode& bytecode;
    const Ast& ast;
    const bool forceOverwrite;
    const bool minimizeDiffs;
    std::string writeBuffer;
    uint32_t indentLevel = 0;
};
