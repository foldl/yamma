msg_Generalappname = MSymbol_AddMsg(sym_General, "appname", "The name `1` is not valid for the application. A valid name starts with a letter followed by letters and digits.");
msg_Generalargrx = MSymbol_AddMsg(sym_General, "argrx", "`1` called with `2` arguments; `3` arguments are expected.");
msg_Generalargtyp = MSymbol_AddMsg(sym_General, "argtyp", "The `2`th argument of `1` is `3`; type `4` is expected.");
msg_Generalargmore = MSymbol_AddMsg(sym_General, "argmore", "`1` called with only `2` arguments; more arguments expected.");
msg_Generalbktmcp = MSymbol_AddMsg(sym_General, "bktmcp", "Expression \"`1`\" has no closing \"`2`\".`4`");
msg_Generalbktwrn = MSymbol_AddMsg(sym_General, "bktwrn", "\"`1`\" represents multiplication; use \"`2`\" to represent a function.`4`");
msg_Generalcatname = MSymbol_AddMsg(sym_General, "catname", "The name `1` is invalid for the category. A valid name starts with a letter followed by letters and digits.");
msg_Generalfile = MSymbol_AddMsg(sym_General, "file", "The preferences file `1` is not a file.");
msg_Generalnewsym = MSymbol_AddMsg(sym_General, "newsym", "Symbol `1` is new.", true);
msg_Generalnosite = MSymbol_AddMsg(sym_General, "nosite", "Site `1` is not an existing Paclet site.");
msg_Generaloffline = MSymbol_AddMsg(sym_General, "offline", "Mathematica is currently configured not to use the Internet. To allow Internet use, check the \"Allow Mathematica to use the Internet\" box in the Help > Internet Connectivity dialog.");
msg_Generaloldversion = MSymbol_AddMsg(sym_General, "oldversion", "Connections to kernels older than Version 2.2 are not supported. This kernel is version `1`.");
msg_Generalpclt = MSymbol_AddMsg(sym_General, "pclt", "`1` does not refer to a known paclet.");
msg_Generalpcltn = MSymbol_AddMsg(sym_General, "pcltn", "No appropriate paclet with name `1` was found.");
msg_Generalpcltni = MSymbol_AddMsg(sym_General, "pcltni", "No appropriate paclet with name `1` is installed.");
msg_Generalpcltnv = MSymbol_AddMsg(sym_General, "pcltnv", "No appropriate paclet with name `1` and version `2` was found.");
msg_Generalpcltnvi = MSymbol_AddMsg(sym_General, "pcltnvi", "No appropriate paclet with name `1` and version `2` is installed.");
msg_Generalpptr = MSymbol_AddMsg(sym_General, "pptr", "`1` does not evaluate to a pair of real numbers at `2` = `3`.");
msg_Generalprefdir = MSymbol_AddMsg(sym_General, "prefdir", "The preferences directory `1` cannot be created.");
msg_Generalshdw = MSymbol_AddMsg(sym_General, "shdw", "Symbol `1` appears in multiple contexts `2`; definitions in context `3` may shadow or be shadowed by other definitions.");
msg_Generalshdwcor = MSymbol_AddMsg(sym_General, "shdwcor", "Symbol `1` defined in `2` already exists in `3`. Select the version you want to use: `4`.");
msg_Generalsntxi = MSymbol_AddMsg(sym_General, "sntxi", "Incomplete expression; more input is needed.`4`");
msg_Generalstop = MSymbol_AddMsg(sym_General, "stop", "Further output of `1` will be suppressed during this calculation.");
msg_Generalstring = MSymbol_AddMsg(sym_General, "string", "String expected at position `1` in `2`.");
msg_Generalstringopt = MSymbol_AddMsg(sym_General, "stringopt", "`1` in `2` is not a string. All options to `3` must be strings.");
msg_Generalstrse = MSymbol_AddMsg(sym_General, "strse", "String or list of strings expected at position `1` in `2`.");
msg_Generalsysmain = MSymbol_AddMsg(sym_General, "sysmain", "Error loading the main binary file `1`. \nGet[\"sysmake.m\"] must be run before continuing.");
msg_Generalwritewarn = MSymbol_AddMsg(sym_General, "writewarn", "Defining rule for `1`.", true);
msg_Generalsntx = MSymbol_AddMsg(sym_General, "sntx", "Invalid syntax near Col.`2` of `3`");

msg_Setsetraw = MSymbol_AddMsg(sym_Set, "setraw", "Cannot assign to raw object `1`.");
msg_Setwrsym  = MSymbol_AddMsg(sym_Set, "wrsym", "Symbol `1` is Protected or Locked.");

msg_Inequalityineq = MSymbol_AddMsg(sym_Inequality, "ineq", "Inequality called with `1` arguments; the number of arguments is expected to be an odd number >= 3."); 

msg_PowerInfy = MSymbol_AddMsg(sym_Message, "infy", "Infinite expression 1/0 encountered.");

msg_MessageName = MSymbol_AddMsg(sym_Message, "name", "Message name `1` is not of the form symbol::name or symbol::name::language.");
