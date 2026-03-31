#pragma once

// ── Frame / app resources ─────────────────────────────────────────────────────
#define IDR_MAINFRAME           128   // app icon + LoadFrame ID (MFC convention)

// ── String table ─────────────────────────────────────────────────────────────
#define IDS_APP_TITLE           101
#define IDS_READY               102
#define IDS_CHECKING            103
#define IDS_DONE                104
#define IDS_UNSAVED_CHANGES     105

// ── Toolbar button command IDs ────────────────────────────────────────────────
#define IDC_BTN_RUN_STOP        201
#define IDC_BTN_SAVE_HTML       202
#define IDC_BTN_SAVE_CFG        203
#define IDC_BTN_RELOAD_CFG      204
#define IDC_BTN_AUTOFIT         205
#define IDC_BTN_CFG_WIZ         206
#define IDC_BTN_INFO            207
#define IDC_BTN_HELP            211
#define IDC_BTN_INFO_SEP        209   // stretch separator before INFO (right-aligned)
#define IDC_BTN_EXIT            208
#define IDC_BTN_VIEW_TOGGLE     210   // toggle list/tab view

// ── Context menu ──────────────────────────────────────────────────────────────
#define IDR_CONTEXT_LIST        401
#define ID_CTX_ENABLE_ALL       401
#define ID_CTX_DISABLE_ALL      402
#define ID_CTX_ENABLE_TCP       405
#define ID_CTX_DISABLE_TCP      406
#define ID_CTX_ENABLE_UDP       407
#define ID_CTX_DISABLE_UDP      408

// ── Dialogs ───────────────────────────────────────────────────────────────────
#define IDD_ABOUT               1003
#define IDD_CONFIG_EDITOR       1010

// ── About dialog controls ─────────────────────────────────────────────────────
#define IDI_ICON_GITHUB         709   // icon_github.ico – used in About dialog
#define IDC_ABOUT_LINK_GITHUB   620
#define IDC_ABOUT_LINK_ICONS8   621
#define IDC_CFG_EDIT_NAME       601
#define IDC_CFG_EDIT_IP         602
#define IDC_CFG_COMBO_TYPE      603
#define IDC_CFG_BTN_ADD_SRV     604
#define IDC_CFG_TAB             606
#define IDC_CFG_LIST            607
#define IDC_CFG_EDIT_PORT       608
#define IDC_CFG_COMBO_PROTO     609
#define IDC_CFG_EDIT_DESC       610
#define IDC_CFG_BTN_ADD_PORT    611
#define IDC_CFG_BTN_UPD_PORT    612
#define IDC_CFG_BTN_DEL_PORT    613
#define IDC_CFG_BTN_IMPORT_CSV  614
#define IDC_CFG_BTN_EXPORT_CSV  615
#define IDC_CFG_BTN_NEW_SRV     616   // always creates new server
#define IDC_CFG_COMBO_TIMEOUT   622   // timeout selector in config editor
#define IDC_CFG_TOOLBAR_SEP     618   // visual separator below toolbar strip
#define IDC_CFG_LBL_NAME        619
#define IDC_CFG_LBL_IP          623
#define IDC_CFG_LBL_TYPE        624




// ── Toolbar icon resource IDs (ICON resources – ICO files embedded in EXE) ───
#define IDI_ICON_RUN      701  // play.ico
#define IDI_ICON_STOP     702  // stop.ico
#define IDI_ICON_HTML     703  // html.ico
#define IDI_ICON_SAVE     704  // cfg_save.ico
#define IDI_ICON_RELOAD   705  // cfg_reload.ico
#define IDI_ICON_CFGEDIT  706  // cfg_edit.ico
#define IDI_ICON_INFO     707  // info.ico
#define IDI_ICON_EXIT     708  // exit.ico
#define IDI_ICON_GITHUB   709  // github.ico – About dialog
#define IDI_ICON_VIEWTABS 710  // view_tabs.ico
#define IDI_ICON_VIEWLIST 711  // view_list.ico
#define IDI_ICON_HELP     712  // help.ico
#define IDI_ICON_SRV_ADD  713  // srv_add.ico
#define IDI_ICON_SAVE2    714  // save.ico

// ── Config-editor toolbar – new icon IDs ─────────────────────────────────────
#define IDI_ICON_CSV_IN    715  // csv_in.ico  (Importar CSV)
#define IDI_ICON_PORT_DEL  716  // port_del.ico
#define IDI_ICON_PORT_EDIT 717  // port_edit.ico
#define IDI_ICON_PORT_ADD  718  // reuses srv_add.ico
#define IDI_ICON_CSV_OUT   719  // csv_out.ico (Exportar CSV)
#define IDI_ICON_SRV_DEL   720  // srv_del.ico (Borrar servidor)

// ── Config-editor toolbar control & command IDs ───────────────────────────────
#define IDC_CFG_TOOLBAR    630

#define TB_NEW_SRV         3001  // Nuevo servidor
#define TB_SAVE_SRV        3002  // Guardar cambios
#define TB_DEL_SRV         3008  // Borrar servidor
#define TB_CSV_IN          3003  // Importar CSV
#define TB_CSV_OUT         3004  // Exportar CSV
#define TB_PORT_ADD        3005  // Agregar puerto
#define TB_PORT_EDIT       3006  // Editar puerto
#define TB_PORT_DEL        3007  // Borrar puerto

// ── Port editor dialog ────────────────────────────────────────────────────────
#define IDD_PORT_EDITOR    1020
#define IDC_PORT_ED_PORT   640
#define IDC_PORT_ED_PROTO  641
#define IDC_PORT_ED_DESC   642

// ── About dialog – version static text ───────────────────────────────────────
#define IDC_ABOUT_VERSION  643
