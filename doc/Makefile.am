pslib.3: pslib.sgml
	docbook2man $<
	mv PSLIB.3 pslib.3

%.3: %.sgml
	docbook-to-man $< > $@

%.html: %.sgml
	docbook2html $<
	mv index.html $@

distclean: clean

summary:
	@for i in $(SGML_FILES); do \
		echo -n "$${i%%.sgml} - "; \
		cat head.xml > $${i%%.sgml}.xml; \
		tail +18 $$i >> $${i%%.sgml}.xml; \
		xsltproc --nonet --novalid extractdesc.xsl $${i%%.sgml}.xml; \
		echo ""; \
		rm $${i%%.sgml}.xml; \
	done

clean:
	rm -f ${man_MANS} $(SGML_FILES:.sgml=.html) pslib.html

SGML_FILES = \
	PS_add_bookmark.sgml \
	PS_add_kerning.sgml \
	PS_add_launchlink.sgml \
	PS_add_ligature.sgml \
	PS_add_locallink.sgml \
	PS_add_note.sgml \
	PS_add_pdflink.sgml \
	PS_add_weblink.sgml \
	PS_arc.sgml \
	PS_arcn.sgml \
	PS_begin_font.sgml \
	PS_begin_glyph.sgml \
	PS_begin_page.sgml \
	PS_begin_pattern.sgml \
	PS_begin_template.sgml \
	PS_boot.sgml \
	PS_circle.sgml \
	PS_clip.sgml \
	PS_closepath_stroke.sgml \
	PS_closepath.sgml \
	PS_close.sgml \
	PS_close_image.sgml \
	PS_continue_text2.sgml \
	PS_continue_text.sgml \
	PS_create_gstate.sgml \
	PS_curveto.sgml \
	PS_delete.sgml \
	PS_end_font.sgml \
	PS_end_glyph.sgml \
	PS_end_page.sgml \
	PS_end_pattern.sgml \
	PS_end_template.sgml \
	PS_fill.sgml \
	PS_fill_stroke.sgml \
	PS_findfont.sgml \
	PS_free_glyph_list.sgml \
	PS_glyph_list.sgml \
	PS_glyph_show.sgml \
	PS_get_buffer.sgml \
	PS_get_majorversion.sgml \
	PS_get_minorversion.sgml \
	PS_get_opaque.sgml \
	PS_get_parameter.sgml \
	PS_get_subminorversion.sgml \
	PS_get_value.sgml \
	PS_hyphenate.sgml \
	PS_include_file.sgml \
	PS_lineto.sgml \
	PS_list_parameters.sgml \
	PS_list_resources.sgml \
	PS_list_values.sgml \
	PS_makespotcolor.sgml \
	PS_moveto.sgml \
	PS_new2.sgml \
	PS_new.sgml \
	PS_open_file.sgml \
	PS_open_fp.sgml \
	PS_open_image.sgml \
	PS_open_image_file.sgml \
	PS_open_mem.sgml \
	PS_place_image.sgml \
	PS_rect.sgml \
	PS_restore.sgml \
	PS_rotate.sgml \
	PS_save.sgml \
	PS_scale.sgml \
	PS_setcolor.sgml \
	PS_setdash.sgml \
	PS_setflat.sgml \
	PS_setfont.sgml \
	PS_setgray.sgml \
	PS_set_info.sgml \
	PS_setlinecap.sgml \
	PS_setlinejoin.sgml \
	PS_setlinewidth.sgml \
	PS_setmiterlimit.sgml \
	PS_setpolydash.sgml \
	PS_set_border_color.sgml \
	PS_set_border_dash.sgml \
	PS_set_border_style.sgml \
	PS_set_gstate.sgml \
	PS_set_parameter.sgml \
	PS_set_text_pos.sgml \
	PS_set_value.sgml \
	PS_shading.sgml \
	PS_shading_pattern.sgml \
	PS_shfill.sgml \
	PS_show2.sgml \
	PS_show_boxed.sgml \
	PS_show.sgml \
	PS_show_xy2.sgml \
	PS_show_xy.sgml \
	PS_shutdown.sgml \
	PS_string_geometry.sgml \
	PS_stringwidth2.sgml \
	PS_stringwidth.sgml \
	PS_stroke.sgml \
	PS_symbol.sgml \
	PS_symbol_name.sgml \
	PS_symbol_width.sgml \
	PS_translate.sgml

EXTRA_DIST = ${SGML_FILES} PS_template.sgml pslib.sgml

man_MANS = $(SGML_FILES:.sgml=.3) pslib.3

html: $(SGML_FILES:.sgml=.html) pslib.html

.PHONY: html
