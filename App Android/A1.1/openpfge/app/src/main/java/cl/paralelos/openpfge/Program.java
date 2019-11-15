package cl.paralelos.openpfge;

public class Program {
    public String name;
    public String programConfig;
    public String brand;
    public String productCode;
    public String userMessage;
    public String source;
    public Boolean defaultProgram;

    public Program(String name, String programConfig, String brand, String productCode, String userMessage, String source, Boolean defaultProgram) {
        this.name = name;
        this.programConfig = programConfig;
        this.brand = brand;
        this.productCode = productCode;
        this.userMessage = userMessage;
        this.source = source;
        this.defaultProgram = defaultProgram;
    }
    public String getProgramDetail(){
        return  programConfig;
    }
}
