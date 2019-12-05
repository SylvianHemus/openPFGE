package cl.paralelos.openpfge;

public class Program {
    public String name;
    public String programConfig;
    public String productBrand;
    public String productCode;
    public String customMessage;
    public String programInfoSource;
    public String sizeRange;
    public Boolean defaultProgram;

    public Program(String name, String programConfig, String productBrand, String productCode, String sizeRange, String customMessage, String programInfoSource, Boolean defaultProgram) {
        this.name = name;
        this.programConfig = programConfig;
        this.productBrand = productBrand;
        this.productCode = productCode;
        this.customMessage = customMessage;
        this.programInfoSource = programInfoSource;
        this.defaultProgram = defaultProgram;
        this.sizeRange=sizeRange;
    }

    public Program(String name, String programConfig, String customMessage) {
        this.name = name;
        this.programConfig = programConfig;
        this.customMessage = customMessage;
        this.defaultProgram = false;
        this.productBrand = null;
        this.productCode = null;
        this.programInfoSource = null;
        this.sizeRange=null;
    }

    public String getProgramDetail() {
        String programDetail = "";
        String lineBreak = "\n";
        programDetail += "Name: " + name + lineBreak;
        if (defaultProgram) {
            programDetail += "Brand: " + productBrand + lineBreak;
            programDetail += "Product code: " + productCode + lineBreak;
            programDetail += "Size range: " + sizeRange + lineBreak;
            programDetail += "Program info. source: " + programInfoSource + lineBreak;
        }
        programDetail += "Default: " + defaultProgram + lineBreak;
        programDetail += "Message: " + customMessage + lineBreak;
        return programDetail;
    }
}
