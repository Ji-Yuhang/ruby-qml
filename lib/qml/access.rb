require 'ostruct'
require 'ropework'
require 'qml/qml'

module QML
  module Access
    def self.included(derived)
      derived.class_eval do
        include Ropework::PropertyDef
        include Ropework::SignalDef
        include InstanceMethods
        extend ClassMethods
      end
    end

    ALLOWED_PATTERN = /^[a-zA-Z_]\w*$/

    module ClassMethods

      def access_support
        @access_composer ||= create_access_support
      end

      private

      def create_access_support
        superclasses = Enumerator.new do |y|
          klass = self
          while klass.include?(Access)
            y << klass
            klass = klass.superclass
          end
        end
        classname = "RubyQml::Access::#{name}"
        
        signals = []
        methods = []
        properties = []
        superclasses.flat_map(&:signals).grep(ALLOWED_PATTERN).each do |name|
          params = signal_defs(include_super: true)[name].args
          fail AccessError, "cannot export variadic signal: #{name}" unless params
          signals << OpenStruct.new(name: name, params: params)
        end
        superclasses.flat_map(&:properties).grep(ALLOWED_PATTERN).each do |name|
          properties << OpenStruct.new(name: name, getter: name, setter: :"#{name}=", notifier: :"#{name}_changed")
          signals << OpenStruct.new(name: :"#{name}_changed", params: [:new_value])
        end
        superclasses.flat_map { |k| k.instance_methods(false) }.grep(ALLOWED_PATTERN).each do |name|
          if signals.find { |signal| signal.name == name } || properties.find { |property| property.getter == name || property.setter == name || property.notifier == name }
            next
          end
          methods << OpenStruct.new(name: name, params: instance_method(name).parameters.map(&:last))
        end
        AccessSupport.new(classname, methods, signals, properties)
      end
    end

    module InstanceMethods
      def initialize(*args, &block)
        super
        signal_names = signals + properties.map { |name| :"#{name}_changed" }
        signal_names.each do |name|
          public_send(name).connect do |*args|
            self.class.access_support.emit_signal(self, name, args)
          end
        end
      end

      def access_object
        (@_access_object ||= ObjectPointer.new).tap do |accessobj|
          self.class.access_support.update_access_object(self, accessobj)
        end
      end
    end
  end
end
