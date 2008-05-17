/* Calf DSP Library
 * API wrappers for Linux audio standards (apart from JACK, which is
 * a separate file now!)
 *
 * Copyright (C) 2007 Krzysztof Foltman
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __CALF_GIFACE_H
#define __CALF_GIFACE_H

#include <stdint.h>
#include <stdlib.h>
#if USE_LADSPA
#include <ladspa.h>
#endif
#if USE_DSSI
#include <dssi.h>
#endif
#if USE_JACK
#include <jack/jack.h>
#include <jack/midiport.h>
#endif
#include <exception>
#include <string>
#include "primitives.h"
#include "preset.h"

struct _cairo;
    
typedef struct _cairo cairo_t;
    
namespace synth {

enum {
    MAX_SAMPLE_RUN = 256
};
    
enum parameter_flags
{
  PF_TYPEMASK = 0x000F,
  PF_FLOAT = 0x0000,
  PF_INT = 0x0001,
  PF_BOOL = 0x0002,
  PF_ENUM = 0x0003,
  PF_ENUM_MULTI = 0x0004, 
  
  PF_SCALEMASK = 0x0FF0,
  PF_SCALE_DEFAULT = 0x0000, ///< no scale given
  PF_SCALE_LINEAR = 0x0010, ///< linear scale
  PF_SCALE_LOG = 0x0020, ///< log scale
  PF_SCALE_GAIN = 0x0030, ///< gain = -96dB..0 or -inf dB
  PF_SCALE_PERC = 0x0040, ///< percent
  PF_SCALE_QUAD = 0x0050, ///< quadratic scale (decent for some gain/amplitude values)

  PF_CTLMASK = 0x0F000,
  PF_CTL_DEFAULT = 0x00000,
  PF_CTL_KNOB = 0x01000,
  PF_CTL_FADER = 0x02000,
  PF_CTL_TOGGLE = 0x03000,
  PF_CTL_COMBO = 0x04000,
  PF_CTL_RADIO = 0x05000,
  PF_CTL_BUTTON = 0x06000,
  
  PF_CTLOPTIONS = 0xFF0000,
  PF_CTLO_HORIZ = 0x010000,
  PF_CTLO_VERT = 0x020000,
  
  PF_UNITMASK = 0xFF000000,
  PF_UNIT_DB = 0x01000000,
  PF_UNIT_COEF = 0x02000000,
  PF_UNIT_HZ = 0x03000000,
  PF_UNIT_SEC = 0x04000000,
  PF_UNIT_MSEC = 0x05000000,
  PF_UNIT_CENTS = 0x06000000,
  PF_UNIT_SEMITONES = 0x07000000,
  PF_UNIT_BPM = 0x08000000,
  PF_UNIT_DEG = 0x09000000,
  PF_UNIT_NOTE = 0x0A000000,
};

class null_audio_module;

struct plugin_command_info
{
    const char *label;
    const char *name;
    const char *description;
};

struct parameter_properties
{
    float def_value, min, max, step;
    uint32_t flags;
    const char **choices;
    const char *short_name, *name;
    float from_01(double value01) const;
    double to_01(float value) const;
    std::string to_string(float value) const;
    int get_char_count() const;
    float get_increment() const;
};

struct line_graph_iface
{
    virtual bool get_graph(int index, int subindex, float *data, int points, cairo_t *context) = 0;
    virtual ~line_graph_iface() {}
};

struct plugin_command_info;

struct plugin_ctl_iface
{
    virtual parameter_properties *get_param_props(int param_no) = 0;
    virtual float get_param_value(int param_no) = 0;
    virtual void set_param_value(int param_no, float value) = 0;
    virtual int get_param_count() = 0;
    virtual const char *get_gui_xml() = 0;
    virtual line_graph_iface *get_line_graph_iface() = 0;
    virtual int get_param_port_offset() = 0;
    virtual bool activate_preset(int bank, int program) = 0;
    virtual const char *get_name() = 0;
    virtual const char *get_id() = 0;
    virtual const char *get_label() = 0;
    virtual int get_input_count()=0;
    virtual int get_output_count()=0;
    virtual bool get_midi()=0;
    virtual float get_level(int port)=0;
    virtual ~plugin_ctl_iface() {}
    virtual plugin_command_info *get_commands() { return NULL; }
    virtual void execute(int cmd_no)=0;
};

struct midi_event {
    uint8_t command;
    uint8_t param1;
    uint16_t param2;
    float param3;
};

struct ladspa_info
{
    uint32_t unique_id;
    const char *label;
    const char *name;
    const char *maker;
    const char *copyright;
    const char *plugin_type;
};

struct giface_plugin_info
{
    ladspa_info *info;
    int inputs, outputs, params;
    bool rt_capable, midi_in_capable;
    parameter_properties *param_props;
};

extern void get_all_plugins(std::vector<giface_plugin_info> &plugins);

#if USE_LADSPA

extern std::string generate_ladspa_rdf(const ladspa_info &info, parameter_properties *params, const char *param_names[], unsigned int count, unsigned int ctl_ofs);

template<class Module>
struct ladspa_instance: public Module, public plugin_ctl_iface
{
    bool activate_flag;
    ladspa_instance()
    {
        for (int i=0; i < Module::in_count; i++)
            Module::ins[i] = NULL;
        for (int i=0; i < Module::out_count; i++)
            Module::outs[i] = NULL;
        for (int i=0; i < Module::param_count; i++)
            Module::params[i] = NULL;
        activate_flag = true;
    }
    virtual parameter_properties *get_param_props(int param_no)
    {
        return &Module::param_props[param_no];
    }
    virtual float get_param_value(int param_no)
    {
        return *Module::params[param_no];
    }
    virtual void set_param_value(int param_no, float value)
    {
        *Module::params[param_no] = value;
    }
    virtual int get_param_count()
    {
        return Module::param_count;
    }
    virtual int get_param_port_offset() 
    {
        return Module::in_count + Module::out_count;
    }
    virtual const char *get_gui_xml() {
        return Module::get_gui_xml();
    }
    virtual line_graph_iface *get_line_graph_iface()
    {
        return this;
    }
    virtual bool activate_preset(int bank, int program) { 
        return false;
    }
    virtual const char *get_name()
    {
        return Module::get_name();
    }
    virtual const char *get_id()
    {
        return Module::get_id();
    }
    virtual const char *get_label()
    {
        return Module::get_label();
    }
    virtual int get_input_count() { return Module::in_count; }
    virtual int get_output_count() { return Module::out_count; }
    virtual bool get_midi() { return Module::support_midi; }
    virtual float get_level(int port) { return 0.f; }
    virtual void execute(int cmd_no) {
        Module::execute(cmd_no);
    }
};

template<class Module>
struct ladspa_wrapper
{
    typedef ladspa_instance<Module> instance;
    
    static LADSPA_Descriptor descriptor;
#if USE_DSSI
    static DSSI_Descriptor dssi_descriptor;
    static DSSI_Program_Descriptor dssi_default_program;

    static std::vector<plugin_preset> *presets;
    static std::vector<DSSI_Program_Descriptor> *preset_descs;
#endif
    ladspa_info &info;
    
    ladspa_wrapper(ladspa_info &i) 
    : info(i)
    {
        init_descriptor(i);
    }

    void init_descriptor(ladspa_info &inf)
    {
        int ins = Module::in_count;
        int outs = Module::out_count;
        int params = Module::param_count;
        descriptor.UniqueID = inf.unique_id;
        descriptor.Label = inf.label;
        descriptor.Name = inf.name;
        descriptor.Maker = inf.maker;
        descriptor.Copyright = inf.copyright;
        descriptor.Properties = Module::rt_capable ? LADSPA_PROPERTY_HARD_RT_CAPABLE : 0;
        descriptor.PortCount = ins + outs + params;
        descriptor.PortNames = new char *[descriptor.PortCount];
        descriptor.PortDescriptors = new LADSPA_PortDescriptor[descriptor.PortCount];
        descriptor.PortRangeHints = new LADSPA_PortRangeHint[descriptor.PortCount];
        int i;
        for (i = 0; i < ins + outs; i++)
        {
            LADSPA_PortRangeHint &prh = ((LADSPA_PortRangeHint *)descriptor.PortRangeHints)[i];
            ((int *)descriptor.PortDescriptors)[i] = i < ins ? LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO
                                                  : i < ins + outs ? LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO
                                                                   : LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            prh.HintDescriptor = 0;
            ((const char **)descriptor.PortNames)[i] = Module::port_names[i];
        }
        for (; i < ins + outs + params; i++)
        {
            LADSPA_PortRangeHint &prh = ((LADSPA_PortRangeHint *)descriptor.PortRangeHints)[i];
            ((int *)descriptor.PortDescriptors)[i] = i < ins ? LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO
                                                  : i < ins + outs ? LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO
                                                                   : LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
            prh.HintDescriptor = LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_BOUNDED_BELOW;
            parameter_properties &pp = Module::param_props[i - ins - outs];
            ((const char **)descriptor.PortNames)[i] = pp.name;
            prh.LowerBound = pp.min;
            prh.UpperBound = pp.max;
            switch(pp.flags & PF_TYPEMASK) {
                case PF_BOOL: 
                    prh.HintDescriptor |= LADSPA_HINT_TOGGLED;
                    break;
                case PF_INT: 
                case PF_ENUM: 
                    prh.HintDescriptor |= LADSPA_HINT_INTEGER;
                    break;
            }
            switch(pp.flags & PF_SCALEMASK) {
                case PF_SCALE_LOG:
                    prh.HintDescriptor |= LADSPA_HINT_LOGARITHMIC;
                    break;
            }
        }
        descriptor.ImplementationData = this;
        descriptor.instantiate = cb_instantiate;
        descriptor.connect_port = cb_connect;
        descriptor.activate = cb_activate;
        descriptor.run = cb_run;
        descriptor.run_adding = NULL;
        descriptor.set_run_adding_gain = NULL;
        descriptor.deactivate = cb_deactivate;
        descriptor.cleanup = cb_cleanup;
#if USE_DSSI
        memset(&dssi_descriptor, 0, sizeof(dssi_descriptor));
        dssi_descriptor.DSSI_API_Version = 1;
        dssi_descriptor.LADSPA_Plugin = &descriptor;
        dssi_descriptor.configure = cb_configure;
        dssi_descriptor.get_program = cb_get_program;
        dssi_descriptor.select_program = cb_select_program;
        dssi_descriptor.run_synth = cb_run_synth;
        
        presets = new std::vector<plugin_preset>;
        preset_descs = new std::vector<DSSI_Program_Descriptor>;

        preset_list plist;
        plist.load_defaults();
        // XXXKF this assumes that plugin name in preset is case-insensitive equal to plugin label
        // if I forget about this, I'll be in a deep trouble
        dssi_default_program.Bank = 0;
        dssi_default_program.Program = 0;
        dssi_default_program.Name = "default";

        int pos = 1;
        for (unsigned int i = 0; i < plist.presets.size(); i++)
        {
            plugin_preset &pp = plist.presets[i];
            if (strcasecmp(pp.plugin.c_str(), descriptor.Label))
                continue;
            DSSI_Program_Descriptor pd;
            pd.Bank = pos >> 7;
            pd.Program = pos++;
            pd.Name = pp.name.c_str();
            preset_descs->push_back(pd);
            presets->push_back(pp);
        }
        // printf("presets = %p:%d name = %s\n", presets, presets->size(), descriptor.Label);
        
#endif
    }

    static LADSPA_Handle cb_instantiate(const struct _LADSPA_Descriptor * Descriptor, unsigned long sample_rate)
    {
        instance *mod = new instance();
        mod->srate = sample_rate;
        return mod;
    }

#if USE_DSSI
    static const DSSI_Program_Descriptor *cb_get_program(LADSPA_Handle Instance, unsigned long index) {
        if (index > presets->size())
            return NULL;
        if (index)
            return &(*preset_descs)[index - 1];
        return &dssi_default_program;
    }
    
    static void cb_select_program(LADSPA_Handle Instance, unsigned long Bank, unsigned long Program) {
        instance *mod = (instance *)Instance;
        unsigned int no = (Bank << 7) + Program - 1;
        // printf("no = %d presets = %p:%d\n", no, presets, presets->size());
        if (no == -1U) {
            for (int i =0 ; i < Module::param_count; i++)
                *mod->params[i] = Module::param_props[i].def_value;
            return;
        }
        if (no >= presets->size())
            return;
        plugin_preset &p = (*presets)[no];
        // printf("activating preset %s\n", p.name.c_str());
        p.activate(mod);
    }
    
#endif
    
    static void cb_connect(LADSPA_Handle Instance, unsigned long port, LADSPA_Data *DataLocation) {
        unsigned long ins = Module::in_count;
        unsigned long outs = Module::out_count;
        unsigned long params = Module::param_count;
        instance *const mod = (instance *)Instance;
        if (port < ins)
            mod->ins[port] = DataLocation;
        else if (port < ins + outs)
            mod->outs[port - ins] = DataLocation;
        else if (port < ins + outs + params) {
            int i = port - ins - outs;
            mod->params[i] = DataLocation;
            *mod->params[i] = Module::param_props[i].def_value;
        }
    }

    static void cb_activate(LADSPA_Handle Instance) {
        instance *const mod = (instance *)Instance;
        mod->activate_flag = true;
    }
    
    static inline void zero_by_mask(Module *module, uint32_t mask, uint32_t offset, uint32_t nsamples)
    {
        for (int i=0; i<Module::out_count; i++) {
            if ((mask & (1 << i)) == 0) {
                dsp::zero(module->outs[i] + offset, nsamples);
            }
        }
    }

    static void cb_run(LADSPA_Handle Instance, unsigned long SampleCount) {
        instance *const mod = (instance *)Instance;
        if (mod->activate_flag)
        {
            mod->set_sample_rate(mod->srate);
            mod->activate();
            mod->activate_flag = false;
        }
        mod->params_changed();
        process_slice(mod, 0, SampleCount);
    }
    
    static inline void process_slice(Module *mod, uint32_t offset, uint32_t end)
    {
        while(offset < end)
        {
            uint32_t newend = std::min(offset + MAX_SAMPLE_RUN, end);
            uint32_t out_mask = mod->process(offset, newend - offset, -1, -1);
            zero_by_mask(mod, out_mask, offset, newend - offset);
            offset = newend;
        }
    }

#if USE_DSSI
    static void cb_run_synth(LADSPA_Handle Instance, unsigned long SampleCount, 
            snd_seq_event_t *Events, unsigned long EventCount) {
        instance *const mod = (instance *)Instance;
        if (mod->activate_flag)
        {
            mod->set_sample_rate(mod->srate);
            mod->activate();
            mod->activate_flag = false;
        }
        mod->params_changed();
        
        uint32_t offset = 0;
        for (uint32_t e = 0; e < EventCount; e++)
        {
            uint32_t timestamp = Events[e].time.tick;
            if (timestamp != offset)
                process_slice(mod, offset, timestamp);
            process_dssi_event(mod, Events[e]);
            offset = timestamp;
        }
        if (offset != SampleCount)
            process_slice(mod, offset, SampleCount);
    }
    
    static char *cb_configure(LADSPA_Handle Instance,
		       const char *Key,
		       const char *Value)
    {
        instance *const mod = (instance *)Instance;
        if (!strcmp(Key, "ExecCommand"))
        {
            if (*Value)
            {
                mod->execute(atoi(Value));
            }
        }
        return NULL;
    }
    
    static void process_dssi_event(Module *module, snd_seq_event_t &event)
    {
        switch(event.type) {
            case SND_SEQ_EVENT_NOTEON:
                module->note_on(event.data.note.note, event.data.note.velocity);
                break;
            case SND_SEQ_EVENT_NOTEOFF:
                module->note_off(event.data.note.note, event.data.note.velocity);
                break;
            case SND_SEQ_EVENT_PGMCHANGE:
                module->program_change(event.data.control.value);
                break;
            case SND_SEQ_EVENT_CONTROLLER:
                module->control_change(event.data.control.param, event.data.control.value);
                break;
            case SND_SEQ_EVENT_PITCHBEND:
                module->pitch_bend(event.data.control.value);
                break;
        }
    }
#endif

    static void cb_deactivate(LADSPA_Handle Instance) {
        instance *const mod = (instance *)Instance;
        mod->deactivate();
    }

    static void cb_cleanup(LADSPA_Handle Instance) {
        instance *const mod = (instance *)Instance;
        delete mod;
    }
    
    std::string generate_rdf() {
        return synth::generate_ladspa_rdf(info, Module::param_props, (const char **)descriptor.PortNames, Module::param_count, Module::in_count + Module::out_count);
    }
};

template<class Module>
LADSPA_Descriptor ladspa_wrapper<Module>::descriptor;

#if USE_DSSI

template<class Module>
DSSI_Descriptor ladspa_wrapper<Module>::dssi_descriptor;

template<class Module>
DSSI_Program_Descriptor ladspa_wrapper<Module>::dssi_default_program;

template<class Module>
std::vector<plugin_preset> *ladspa_wrapper<Module>::presets;

template<class Module>
std::vector<DSSI_Program_Descriptor> *ladspa_wrapper<Module>::preset_descs;
#endif


#endif

struct audio_exception: public std::exception
{
    const char *text;
    std::string container;
public:
    audio_exception(const std::string &t) : container(t) { text = container.c_str(); }
    virtual const char *what() const throw () { return text; }
    virtual ~audio_exception() throw () {}
};

// this should go into some utility library, perhaps
std::string xml_escape(const std::string &src);

};

#endif
